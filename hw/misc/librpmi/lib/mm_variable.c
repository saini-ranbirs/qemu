// SPDX-License-Identifier: BSD-2-Clause
/*
 * Copyright (c) 2025 Ventana Micro Systems Inc.
 */

#include <librpmi.h>
#include "librpmi_internal.h"
#include "mm_variable.h"

#include <Guid/SmmVariableCommon.h>

static rpmi_uint32_t max_variable_size = 0x2800;
static rpmi_uint8_t* m_var_bfr_payload = NULL;
static rpmi_uint32_t m_var_bfr_payload_size = 0;

#define mVariableBufferPayload      m_var_bfr_payload
#define mVariableBufferPayloadSize  m_var_bfr_payload_size
#define CommBufferPayloadSize       comm_bfr_payload_size

extern char *riscv_var_fd;
UINT8 mVarCount;
UINT8 mVarBuffer[45][700];
UINT8 mVarCurr = 0;

EFI_GUID gEfiVariableGuid = EFI_VARIABLE_GUID;
EFI_GUID gEfiAuthenticatedVariableGuid = EFI_AUTHENTICATED_VARIABLE_GUID;

EFI_STATUS
EFIAPI
VariableServiceGetVariable (
  IN      CHAR16    *VariableName,
  IN      EFI_GUID  *VendorGuid,
  OUT     UINT32    *Attributes OPTIONAL,
  IN OUT  UINTN     *DataSize,
  OUT     VOID      *Data OPTIONAL
  )
{
  EFI_STATUS                                Status;
  UINTN                                     VarDataSize;
  SMM_VARIABLE_COMMUNICATE_ACCESS_VARIABLE  *pSVCAVar;

  if ((VariableName == NULL) || (VendorGuid == NULL) || (DataSize == NULL)) {
    DPRINTF("Ranbir: GNVNI Status1 L%d = EFI_INVALID_PARAMETER\n", __LINE__);
    return EFI_INVALID_PARAMETER;
  }

  if (VariableName[0] == 0) {
    DPRINTF("Ranbir: GNVNI Status1 L%d = EFI_NOT_FOUND\n", __LINE__);
    return EFI_NOT_FOUND;
  }

  //AcquireLockOnlyAtBootTime (&mVariableModuleGlobal->VariableGlobal.VariableServicesLock);

  mVarCurr = 0;
  Status = FindVariableRS (VariableName, VendorGuid);
  DPRINTF("Ranbir: GNVNI Status1 L%d = 0x%llx CurrPtr = %d mVarCount = %d\n",
          __LINE__, Status, mVarCurr, mVarCount);
  if (EFI_ERROR (Status) || (mVarCurr >= mVarCount)) {
    goto Done;
  }

  //
  // Get data size
  //
  pSVCAVar = (SMM_VARIABLE_COMMUNICATE_ACCESS_VARIABLE *)&mVarBuffer[mVarCurr];
  VarDataSize = pSVCAVar->DataSize;

  if (VarDataSize && *DataSize >= VarDataSize) {
    if (Data == NULL) {
      Status = EFI_INVALID_PARAMETER;
      goto Done;
    }

    CopyMem (Data, (UINT8 *)pSVCAVar->Name + pSVCAVar->NameSize, VarDataSize);

    *DataSize = VarDataSize;

    Status = EFI_SUCCESS;
  } else {
    *DataSize = VarDataSize;
    Status    = EFI_BUFFER_TOO_SMALL;
  }

  if ((Status == EFI_SUCCESS) || (Status == EFI_BUFFER_TOO_SMALL)) {
    if ((Attributes != NULL) && (mVarCurr < mVarCount)) {
      *Attributes = pSVCAVar->Attributes;
    }
  }

Done:
  //ReleaseLockOnlyAtBootTime (&mVariableModuleGlobal->VariableGlobal.VariableServicesLock);
  return Status;
}

EFI_STATUS
EFIAPI
VariableServiceGetNextVariableInternal (
  IN  CHAR16                 *VariableName,
  IN  EFI_GUID               *VendorGuid,
  IN  VARIABLE_STORE_HEADER  **VariableStoreList,
  OUT VARIABLE_HEADER        **VariablePtr,
  IN  BOOLEAN                AuthFormat
  )
{
  EFI_STATUS  Status;

  Status = EFI_NOT_FOUND;

  // Check if the variable exists in the given variable store list
  /*DPRINTF("Ranbir: L%d StoreType = %d initiating memdump\n", __LINE__, StoreType));
  if ((Variable.EndPtr - Variable.StartPtr) > 0)
    memdump(Variable.StartPtr, Variable.EndPtr - Variable.StartPtr + 1, "ALL");
  else
    memdump(Variable.StartPtr, Variable.StartPtr - Variable.EndPtr + 1, "ALL");*/

  mVarCurr = 0;
  Status = FindVariableRS (VariableName, VendorGuid);
  DPRINTF("Ranbir: GNVNI Status1 L%d = 0x%llx CurrPtr = %d mVarCount = %d\n",
          __LINE__, Status, mVarCurr, mVarCount);

  if (EFI_ERROR (Status)) {
    //
    // For VariableName is an empty string, FindVariableEx() will try to find and return
    // the first qualified variable, and if FindVariableEx() returns error (EFI_NOT_FOUND)
    // as no any variable is found, still go to return the error (EFI_NOT_FOUND).
    //
    if (VariableName[0] != 0) {
      //
      // For VariableName is not an empty string, and FindVariableEx() returns error as
      // VariableName and VendorGuid are not a name and GUID of an existing variable,
      // there is no way to get next variable, follow spec to return EFI_INVALID_PARAMETER.
      //
      Status = EFI_INVALID_PARAMETER;
      DPRINTF("Ranbir: GNVNI Status1 L%d = 0x%llx ", __LINE__, Status); // Not coming here
    }

    DPRINTF("Ranbir: GNVNI Status1 L%d = 0x%llx\n", __LINE__, Status);
    goto Done;
  }

  if (VariableName[0] != 0) {
    //
    // If variable name is not empty, get next variable.
    //
    mVarCurr++;
    if (mVarCurr >= mVarCount)
      Status = EFI_NOT_FOUND;

    DPRINTF("Ranbir: GNVNI Status1 L%d = 0x%llx\n", __LINE__, Status);
    goto Done;
  }

Done:
  return Status;
}

EFI_STATUS
EFIAPI
VariableServiceGetNextVariableName (
  IN OUT  UINTN     *VariableNameSize,
  IN OUT  CHAR16    *VariableName,
  IN OUT  EFI_GUID  *VendorGuid
  )
{
  EFI_STATUS             Status;
  UINTN                  MaxLen;
  UINTN                  VarNameSize;
  BOOLEAN                AuthFormat;
  VARIABLE_HEADER        *VariablePtr;
  VARIABLE_STORE_HEADER  *VariableStoreHeader[VariableStoreTypeMax];
  static UINTN           CallCount = 0;

  CallCount++;
  if ((VariableNameSize == NULL) || (VariableName == NULL) || (VendorGuid == NULL)) {
    CallCount--;
    return EFI_INVALID_PARAMETER;
  }

  AuthFormat = FALSE;

  //
  // Calculate the possible maximum length of name string, including the Null terminator.
  //
  MaxLen = *VariableNameSize / sizeof (CHAR16);
  if ((MaxLen == 0) || (StrnLenS (VariableName, MaxLen) == MaxLen)) {
    //
    // Null-terminator is not found in the first VariableNameSize bytes of the input VariableName buffer,
    // follow spec to return EFI_INVALID_PARAMETER.
    //
    CallCount--;
    return EFI_INVALID_PARAMETER;
  }

  //AcquireLockOnlyAtBootTime (&mVariableModuleGlobal->VariableGlobal.VariableServicesLock);

  Status =  VariableServiceGetNextVariableInternal (
              VariableName,
              VendorGuid,
              VariableStoreHeader,
              &VariablePtr,
              AuthFormat
              );
  if (!EFI_ERROR (Status)) {
    SMM_VARIABLE_COMMUNICATE_ACCESS_VARIABLE  *pSVCAVar;

    pSVCAVar = (SMM_VARIABLE_COMMUNICATE_ACCESS_VARIABLE *)&mVarBuffer[mVarCurr];
    VarNameSize = pSVCAVar->NameSize;
    ASSERT (VarNameSize != 0);
    if (VarNameSize <= *VariableNameSize) {
      CopyMem (
        VariableName,
        pSVCAVar->Name,
        VarNameSize
        );
      CopyMem (
        VendorGuid,
        (const void *)pSVCAVar,
        sizeof (EFI_GUID)
        );
      Status = EFI_SUCCESS;
    } else {
      Status = EFI_BUFFER_TOO_SMALL;
    }

    *VariableNameSize = VarNameSize;
  }

  //ReleaseLockOnlyAtBootTime (&mVariableModuleGlobal->VariableGlobal.VariableServicesLock);
  return Status;
}

EFI_STATUS
FindVariableRS (
  IN CHAR16    *VariableName,
  IN EFI_GUID  *VendorGuid
  )
{
  EFI_STATUS                                Status;
  SMM_VARIABLE_COMMUNICATE_ACCESS_VARIABLE  *pSVCAVar;

  DPRINTF("Ranbir: FindVariableRS L%d = START mVarCount = %d\n", __LINE__, mVarCount);
  while (mVarCurr < mVarCount) {
    if (VariableName[0] == 0) {
      DPRINTF("Ranbir: FindVariableRS L%d = EFI_SUCCESS mVarCurr = %d\n", __LINE__, mVarCurr);
      return EFI_SUCCESS;
    } else {
      pSVCAVar = (SMM_VARIABLE_COMMUNICATE_ACCESS_VARIABLE *)&mVarBuffer[mVarCurr];
      memdump(pSVCAVar, 16, "1");
      if (CompareGuid (VendorGuid, (const GUID *)pSVCAVar)) {
        ASSERT ((pSVCAVar->NameSize) != 0);
        memdump(pSVCAVar->Name, pSVCAVar->NameSize, "1");
        if (CompareMem (VariableName, pSVCAVar->Name, pSVCAVar->NameSize) == 0) {
          DPRINTF("Ranbir: FindVariableRS L%d = EFI_SUCCESS mVarCurr = %d\n", __LINE__, mVarCurr);
          return EFI_SUCCESS;
        } else {
          DPRINTF("Ranbir: FindVariableRS Variable Name Mismatch L%d\n", __LINE__);
        }
      } else {
        DPRINTF("Ranbir: FindVariableRS Vendor GUID Mismatch L%d\n", __LINE__);
      }
    }
    mVarCurr++;
  }

  Status = (mVarCurr >= mVarCount) ? EFI_NOT_FOUND : EFI_SUCCESS;
  DPRINTF("Ranbir: FindVariableRS L%d = %s mVarCurr = %d\n", __LINE__, Status ? "EFI_NOT_FOUND" : "EFI_SUCCESS", mVarCurr);
  return Status;
}

void mm_memdump(const void *src, rpmi_size_t count, const char *name)
{
#define BFR_DATA_LIMIT  16

	const char *temp = src;
	unsigned char bfr_data[BFR_DATA_LIMIT];
	size_t bfr_counter, bfr_counter_limit;
	size_t remaining = count, loop_count = 0;

	if (!remaining)
		return;

	DPRINTF("\nSMM %s : %06lu", name, count);

	while (remaining) {
		bfr_counter = 0;

		(remaining >= BFR_DATA_LIMIT) ?
		    (bfr_counter_limit = BFR_DATA_LIMIT) :
		    (bfr_counter_limit = remaining);

		while (bfr_counter < bfr_counter_limit) {
			bfr_data[bfr_counter] = *(temp + bfr_counter);
			bfr_counter++;
		}

		/* For simplicity, fill rest with ZERO's if required */
		while (bfr_counter < BFR_DATA_LIMIT) {
			bfr_data[bfr_counter] = 0x00;
			bfr_counter++;
		}

		if (loop_count < 25) {
		DPRINTF("\n%06lu "
			"%02X%02X %02X%02X %02X%02X %02X%02X "
			"%02X%02X %02X%02X %02X%02X %02X%02X",
			count - remaining,
			bfr_data[1], bfr_data[0], bfr_data[3], bfr_data[2],
			bfr_data[5], bfr_data[4], bfr_data[7], bfr_data[6],
			bfr_data[9], bfr_data[8], bfr_data[11], bfr_data[10],
			bfr_data[13], bfr_data[12], bfr_data[15], bfr_data[14]);
		}

		temp = temp + bfr_counter_limit;
		remaining = remaining - bfr_counter_limit;
		loop_count++;
	}

	DPRINTF("\n%06lu\n", count - remaining);
}

enum rpmi_error mm_variable_init(void)
{
	m_var_bfr_payload_size =
	    max_variable_size +
	    offsetof(SMM_VARIABLE_COMMUNICATE_VAR_CHECK_VARIABLE_PROPERTY, Name)
	    - sizeof(AUTHENTICATED_VARIABLE_HEADER);

	m_var_bfr_payload = rpmi_env_zalloc(m_var_bfr_payload_size);
	if (!m_var_bfr_payload) {
		DPRINTF("====================================================> "
			"%s() failed to allocate memory \n", __func__ );
		return RPMI_ERR_FAILED;
	}

	DPRINTF("====================================================> "
		"%s() memory allocated for buffer \n", __func__ );

	return RPMI_SUCCESS;
}

static inline UINTN fxn_get_payload_size(rpmi_uint8_t *comm_hdr_data,
					 rpmi_uint32_t bfr_payload_size)
{
	SMM_VARIABLE_COMMUNICATE_GET_PAYLOAD_SIZE *get_payload_size;

	DPRINTF("====================================================> "
		"SmmVariableFunctionHeader->function = "
		"SMM_VARIABLE_FUNCTION_GET_PAYLOAD_SIZE.\n" );

	if (bfr_payload_size <
	    sizeof(SMM_VARIABLE_COMMUNICATE_GET_PAYLOAD_SIZE))
		return EFI_INVALID_PARAMETER;

	get_payload_size =
	    (SMM_VARIABLE_COMMUNICATE_GET_PAYLOAD_SIZE *)comm_hdr_data;
	get_payload_size->VariablePayloadSize = m_var_bfr_payload_size;

	DPRINTF("====================================================> "
		"%s: get_payload_size->variable_payload_size = 0x%x \n",
		__func__, m_var_bfr_payload_size);

	return EFI_SUCCESS;
}

#if 0
rpmi_uint8_t svar_data[786432];
static rpmi_bool_t first_time = TRUE;

static int get_variable_from_variable_store(SMM_VARIABLE_COMMUNICATE_ACCESS_VARIABLE *smm_var_hdr)
{
	return 0;

	if (first_time) {
		get_data_from_secure_variable_fd(svar_data);
		first_time = FALSE;
	}

	// Vendor Guid
	DPRINTF("====================================================> "
		"%s: Vendor Guid \n", __func__);
	mm_memdump((void *)&smm_var_hdr->Guid, sizeof(smm_var_hdr->Guid), "Guid");

	DPRINTF("====================================================> "
		"%s: Var Name Size = %ld \n", __func__, smm_var_hdr->NameSize);

	DPRINTF("====================================================> "
		"%s: Var Name \n", __func__);
	mm_memdump(smm_var_hdr->Name, smm_var_hdr->NameSize, "Name");

	DPRINTF("====================================================> "
		"%s: Flash dump follows \n", __func__);
	mm_memdump((void *)svar_data, 0x170, "");

	/*{
		EFI_FIRMWARE_VOLUME_HEADER *fvh;
		VARIABLE_STORE_HEADER *vsh;

		fvh = (EFI_FIRMWARE_VOLUME_HEADER *)svar_data;
		vsh = (VARIABLE_STORE_HEADER *)&svar_data[72];

		DPRINTF("secure-var: FVH = %p VSH = %p\n",
			(void *)fvh, (void *)vsh);
		DPRINTF("secure-var: Store Status = %d\n",
			GetVariableStoreStatus(vsh);
	}*/

	DPRINTF("====================================================> "
		"%s: Start Pointer = %p \n", __func__,
		GetStartPointer((VARIABLE_STORE_HEADER *)&svar_data[72]));
	mm_memdump((void *)&svar_data[72], 0x170 - 72, "");
	DPRINTF("====================================================> "
		"%s: End   Pointer = %p, \n", __func__,
		GetEndPointer((VARIABLE_STORE_HEADER *)&svar_data[72]));

	//FindVariableEx();

	return 0;
}

static inline UINTN fxn_get_variable(rpmi_uint8_t *comm_hdr_data,
				     rpmi_uint32_t bfr_payload_size)
{
	SMM_VARIABLE_COMMUNICATE_ACCESS_VARIABLE *smm_var_hdr;
	rpmi_uint64_t info_size;

	DPRINTF("====================================================> "
		"%s: ENTER \n", __func__);

	DPRINTF("====================================================> "
		"SmmVariableFunctionHeader->function = "
		"SMM_VARIABLE_FUNCTION_GET_VARIABLE %lu \n",
		sizeof(SMM_VARIABLE_COMMUNICATE_ACCESS_VARIABLE));

	if (bfr_payload_size <
	    offsetof(SMM_VARIABLE_COMMUNICATE_ACCESS_VARIABLE, Name))
		return EFI_INVALID_PARAMETER;

	smm_var_hdr =
	    (SMM_VARIABLE_COMMUNICATE_ACCESS_VARIABLE *)m_var_bfr_payload;

	//
	// Prevent info_size overflow
	//
	if (((rpmi_uint64_t)(~0) -
	     smm_var_hdr->DataSize <
	     offsetof(SMM_VARIABLE_COMMUNICATE_ACCESS_VARIABLE, Name)) ||
	    ((rpmi_uint64_t)(~0) -
	     smm_var_hdr->NameSize <
	     offsetof(SMM_VARIABLE_COMMUNICATE_ACCESS_VARIABLE, Name) +
	     smm_var_hdr->DataSize))
		return EFI_INVALID_PARAMETER;

	info_size = offsetof(SMM_VARIABLE_COMMUNICATE_ACCESS_VARIABLE, Name) +
		    smm_var_hdr->DataSize + smm_var_hdr->NameSize;

	//
	// range check already covered before
	//
	if (info_size > bfr_payload_size)
		return EFI_INVALID_PARAMETER;

	//
	// The VariableSpeculationBarrier() call here is to ensure the previous
	// range/content checks for the CommBuffer have been completed before the
	// subsequent consumption of the CommBuffer content.
	//
	//VariableSpeculationBarrier ();
	if ((smm_var_hdr->NameSize < sizeof(rpmi_int16_t)) ||
	    (smm_var_hdr->Name[smm_var_hdr->NameSize / sizeof(rpmi_int16_t) - 1] != '\0')) {
		//
		// Make sure VariableName is A Null-terminated string.
		//
		return EFI_INVALID_PARAMETER;
	}

	get_variable_from_variable_store(smm_var_hdr);

	DPRINTF("====================================================> "
		"%s: EXIT %d \n", __func__, __LINE__);

	//CopyMem(SmmVariableFunctionHeader->Data, mVariableBufferPayload, CommBufferPayloadSize);

	return EFI_NOT_FOUND;
}

static UINTN set_variable_to_variable_store(SMM_VARIABLE_COMMUNICATE_ACCESS_VARIABLE *smm_var_hdr)
{
	EFI_STATUS              Status;
	VARIABLE_POINTER_TRACK  Variable;

	if (first_time) {
		get_data_from_secure_variable_fd(svar_data);
		first_time = FALSE;
	}

	Status = EFI_NOT_FOUND;

	ZeroMem (&Variable, sizeof (Variable));

	Variable.StartPtr = GetStartPointer((VARIABLE_STORE_HEADER *)&svar_data[72]);
	Variable.EndPtr   = GetEndPointer((VARIABLE_STORE_HEADER *)&svar_data[72]);
	Variable.Volatile = TRUE;

	Status = FindVariableEx((void *)&smm_var_hdr->Name, (void *)&smm_var_hdr->Guid, FALSE, &Variable, TRUE);
	if (!EFI_ERROR (Status)) {
		DPRINTF("====================================================> "
			"%s: Flash dump after set follows \n", __func__);

		mm_memdump((void *)svar_data, 0x170, "");
	}

	return 0;
}

static inline UINTN fxn_set_variable(rpmi_uint8_t *comm_hdr_data,
				     rpmi_uint32_t bfr_payload_size)
{
	SMM_VARIABLE_COMMUNICATE_ACCESS_VARIABLE *smm_var_hdr;

	DPRINTF("====================================================> "
		"SmmVariableFunctionHeader->function = "
		"SMM_VARIABLE_FUNCTION_SET_VARIABLE \n");

	smm_var_hdr =
	    (SMM_VARIABLE_COMMUNICATE_ACCESS_VARIABLE *)m_var_bfr_payload;

	set_variable_to_variable_store(smm_var_hdr);

	return EFI_SUCCESS;
}
#endif

enum rpmi_error mm_variable_handler(IN OUT VOID   *CommBuffer,
				    IN OUT UINTN  *CommBufferSize)
{
	SMM_VARIABLE_COMMUNICATE_HEADER *SmmVariableFunctionHeader;
	SMM_VARIABLE_COMMUNICATE_ACCESS_VARIABLE *SmmVariableHeader;
	SMM_VARIABLE_COMMUNICATE_GET_NEXT_VARIABLE_NAME *GetNextVariableName;
	SMM_VARIABLE_COMMUNICATE_GET_PAYLOAD_SIZE *GetPayloadSize;
	UINTN Status = EFI_SUCCESS;
	UINTN InfoSize;
	UINTN NameBufferSize;
	UINTN comm_bfr_payload_size;
	UINTN TempCommBufferSize;

	DPRINTF("====================================================> "
		"%s: ENTER \n", __func__);

	//
	// If input is invalid, stop processing this SMI
	//
	if ((CommBuffer == NULL)) {
		return RPMI_SUCCESS;
	}

	TempCommBufferSize = *CommBufferSize;

	if (TempCommBufferSize < SMM_VARIABLE_COMMUNICATE_HEADER_SIZE) {
		DPRINTF("mm_variable_handler: SMM communication buffer size "
			"invalid!\n");
		return RPMI_SUCCESS;
	}

	CommBufferPayloadSize =
	    TempCommBufferSize - SMM_VARIABLE_COMMUNICATE_HEADER_SIZE;
	if (CommBufferPayloadSize > mVariableBufferPayloadSize) {
		DPRINTF("mm_variable_handler: SMM communication buffer payload "
			"size invalid!\n");
		return RPMI_SUCCESS;
	}

	SmmVariableFunctionHeader = (SMM_VARIABLE_COMMUNICATE_HEADER *)CommBuffer;
	DPRINTF("====================================================> "
		"%s: SmmVariableFunctionHeader->function = 0x%llx \n",
		__func__, SmmVariableFunctionHeader->Function);

	mm_memdump(SmmVariableFunctionHeader, TempCommBufferSize, "H");
	DPRINTF("====================================================> "
		"%s: CommBufferPayloadSize = %lld \n",
		__func__, CommBufferPayloadSize);

	mm_memdump(SmmVariableFunctionHeader->Data, CommBufferPayloadSize, "D");

	switch (SmmVariableFunctionHeader->Function) {
	case SMM_VARIABLE_FUNCTION_GET_PAYLOAD_SIZE:
		DPRINTF("Ranbir: Processing SMM_VARIABLE_FUNCTION_GET_PAYLOAD_SIZE\n");
		if (CommBufferPayloadSize <
		    sizeof(SMM_VARIABLE_COMMUNICATE_GET_PAYLOAD_SIZE)) {
			DPRINTF("GetPayloadSize: SMM communication buffer "
				"size invalid!\n");
			Status = EFI_INVALID_PARAMETER;
			break;
		}

		GetPayloadSize                      = (SMM_VARIABLE_COMMUNICATE_GET_PAYLOAD_SIZE *)SmmVariableFunctionHeader->Data;
		GetPayloadSize->VariablePayloadSize = mVariableBufferPayloadSize;
		Status                              = EFI_SUCCESS;

		/*status = fxn_get_payload_size(SmmVariableFunctionHeader->Data,
					      CommBufferPayloadSize);*/
		break;

	case SMM_VARIABLE_FUNCTION_GET_VARIABLE:
		DPRINTF("Ranbir: Processing SMM_VARIABLE_FUNCTION_GET_VARIABLE\n");
		if (CommBufferPayloadSize <
		    OFFSET_OF(SMM_VARIABLE_COMMUNICATE_ACCESS_VARIABLE, Name)) {
			DPRINTF("GetVariable: SMM communication buffer "
				"size invalid!\n");
			Status = EFI_INVALID_PARAMETER;
			break;
		}

		//
		// Copy the input communicate buffer payload to pre-allocated SMM variable buffer payload.
		//
		CopyMem(mVariableBufferPayload, SmmVariableFunctionHeader->Data,
			CommBufferPayloadSize);
		SmmVariableHeader =
		    (SMM_VARIABLE_COMMUNICATE_ACCESS_VARIABLE *)
		    mVariableBufferPayload;
		if (((UINTN)(~0) - SmmVariableHeader->DataSize <
		     OFFSET_OF(SMM_VARIABLE_COMMUNICATE_ACCESS_VARIABLE, Name))
		    ||
		    ((UINTN)(~0) - SmmVariableHeader->NameSize <
		     OFFSET_OF(SMM_VARIABLE_COMMUNICATE_ACCESS_VARIABLE, Name) +
		     SmmVariableHeader->DataSize)) {
			//
			// Prevent InfoSize overflow happen
			//
			DPRINTF("GetVariable: InfoSize overflow!\n");
			Status = EFI_ACCESS_DENIED;
			break;
		}

		InfoSize =
		    OFFSET_OF(SMM_VARIABLE_COMMUNICATE_ACCESS_VARIABLE, Name) +
		    SmmVariableHeader->DataSize + SmmVariableHeader->NameSize;

		//
		// SMRAM range check already covered before
		//
		if (InfoSize > CommBufferPayloadSize) {
			DPRINTF("GetVariable: Data size exceed communication "
				"buffer size limit!\n");
			Status = EFI_ACCESS_DENIED;
			break;
		}

		//
		// The VariableSpeculationBarrier() call here is to ensure the previous
		// range/content checks for the CommBuffer have been completed before the
		// subsequent consumption of the CommBuffer content.
		//
		//VariableSpeculationBarrier ();
		if ((SmmVariableHeader->NameSize < sizeof (CHAR16)) ||
		    (SmmVariableHeader->
		     Name[SmmVariableHeader->NameSize/sizeof (CHAR16) - 1] !=
		     L'\0')) {
			//
			// Make sure VariableName is A Null-terminated string.
			//
			DPRINTF("GetVariable: VariableName NOT Null-terminated!\n");
			Status = EFI_ACCESS_DENIED;
			break;
		}

		DPRINTF("Ranbir: VariableServiceGetVariable being called\n");
		Status =
		    VariableServiceGetVariable(SmmVariableHeader->Name,
					       &SmmVariableHeader->Guid,
					       &SmmVariableHeader->Attributes,
					       &SmmVariableHeader->DataSize,
					       (UINT8 *)SmmVariableHeader->Name
					       + SmmVariableHeader->NameSize);
		//CopyMem(SmmVariableFunctionHeader->Data, &mVarBuffer[mVarCurr],
		//	CommBufferPayloadSize);
		rpmi_env_writeb((rpmi_uint64_t)SmmVariableFunctionHeader->Data,
				(rpmi_uint8_t *)&mVarBuffer[mVarCurr],
				CommBufferPayloadSize);
		DPRINTF("Ranbir: SmmVariableHeader->DataSize = %lld\n",
			SmmVariableHeader->DataSize);
		mm_memdump(SmmVariableFunctionHeader->Data, CommBufferPayloadSize, "OUT");
		break;

	case SMM_VARIABLE_FUNCTION_GET_NEXT_VARIABLE_NAME:
		DPRINTF("Ranbir: Processing SMM_VARIABLE_FUNCTION_GET_NEXT_VARIABLE_NAME\n");
		{
			static BOOLEAN bFirstTime = TRUE;

			if (bFirstTime) {
				UINT8 Count = mVarCount;

				while (Count) {
					mm_memdump(&mVarBuffer[Count - 1], CommBufferPayloadSize, "VCACHE");
					Count--;
				}

				bFirstTime = FALSE;
			}
		}
		if (CommBufferPayloadSize <
		    OFFSET_OF(SMM_VARIABLE_COMMUNICATE_GET_NEXT_VARIABLE_NAME,
			      Name)) {
			DPRINTF("GetNextVariableName: SMM communication buffer "
				"size invalid!\n");
			Status = EFI_SUCCESS;
			break;
		}

		//
		// Copy the input communicate buffer payload to pre-allocated SMM variable buffer payload.
		//
		CopyMem(mVariableBufferPayload, SmmVariableFunctionHeader->Data,
			CommBufferPayloadSize);
		mm_memdump(SmmVariableFunctionHeader->Data, CommBufferPayloadSize, "IN");
		GetNextVariableName =
		    (SMM_VARIABLE_COMMUNICATE_GET_NEXT_VARIABLE_NAME *)
		    mVariableBufferPayload;
		mm_memdump(&GetNextVariableName->Guid, 16, "V");
		mm_memdump(GetNextVariableName->Name, 46, "N");
		if ((UINTN)(~0) - GetNextVariableName->NameSize <
		    OFFSET_OF(SMM_VARIABLE_COMMUNICATE_GET_NEXT_VARIABLE_NAME,
			      Name)) {
			//
			// Prevent InfoSize overflow happen
			//
			DPRINTF("GetNextVariableName: InfoSize overflow!\n");
			Status = EFI_ACCESS_DENIED;
			break;
		}

		InfoSize =
		    OFFSET_OF(SMM_VARIABLE_COMMUNICATE_GET_NEXT_VARIABLE_NAME,
			      Name) + GetNextVariableName->NameSize;

		//
		// SMRAM range check already covered before
		//
		if (InfoSize > CommBufferPayloadSize) {
			DPRINTF("GetNextVariableName: Data size exceed "
				"communication buffer size limit!\n");
			Status = EFI_ACCESS_DENIED;
			break;
		}

		NameBufferSize =
		    CommBufferPayloadSize -
		    OFFSET_OF(SMM_VARIABLE_COMMUNICATE_GET_NEXT_VARIABLE_NAME,
			      Name);
		if ((NameBufferSize < sizeof (CHAR16)) ||
		    (GetNextVariableName->
		     Name[NameBufferSize/sizeof (CHAR16) - 1] != L'\0')) {
			//
			// Make sure input VariableName is A Null-terminated string.
			//
			DPRINTF("GetNextVariableName: VariableName NOT Null-terminated!\n");
			Status = EFI_ACCESS_DENIED;
			break;
		}

		DPRINTF("Ranbir: VariableServiceGetNextVariableName being called\n");
		Status =
		    VariableServiceGetNextVariableName
		    (&GetNextVariableName->NameSize, GetNextVariableName->Name,
		     &GetNextVariableName->Guid);
		//CopyMem(SmmVariableFunctionHeader->Data, mVariableBufferPayload,
		//	CommBufferPayloadSize);
		rpmi_env_writeb((rpmi_uint64_t)SmmVariableFunctionHeader->Data,
				(rpmi_uint8_t *)mVariableBufferPayload,
				CommBufferPayloadSize);
		DPRINTF("Ranbir: VariableServiceGetNextVariableName call returned\n");
		mm_memdump(SmmVariableFunctionHeader->Data, CommBufferPayloadSize, "OUT");
		break;

	case SMM_VARIABLE_FUNCTION_SET_VARIABLE:
		DPRINTF("Ranbir: Processing SMM_VARIABLE_FUNCTION_SET_VARIABLE\n");
		if (CommBufferPayloadSize <
		    OFFSET_OF(SMM_VARIABLE_COMMUNICATE_ACCESS_VARIABLE, Name)) {
			DPRINTF("SetVariable: SMM communication buffer "
				"size invalid!\n");
			Status = EFI_SUCCESS;
			break;
		}

		//
		// Copy the input communicate buffer payload to pre-allocated SMM variable buffer payload.
		//
		CopyMem(mVariableBufferPayload, SmmVariableFunctionHeader->Data,
			CommBufferPayloadSize);
		SmmVariableHeader =
		    (SMM_VARIABLE_COMMUNICATE_ACCESS_VARIABLE *)
		    mVariableBufferPayload;
		if (((UINTN)(~0) - SmmVariableHeader->DataSize <
		     OFFSET_OF(SMM_VARIABLE_COMMUNICATE_ACCESS_VARIABLE, Name))
		    ||
		    ((UINTN)(~0) - SmmVariableHeader->NameSize <
		     OFFSET_OF(SMM_VARIABLE_COMMUNICATE_ACCESS_VARIABLE, Name) +
		     SmmVariableHeader->DataSize)) {
			//
			// Prevent InfoSize overflow happen
			//
			DPRINTF("SetVariable: InfoSize overflow!\n");
			Status = EFI_ACCESS_DENIED;
			break;
		}

		InfoSize =
		    OFFSET_OF(SMM_VARIABLE_COMMUNICATE_ACCESS_VARIABLE, Name) +
		    SmmVariableHeader->DataSize + SmmVariableHeader->NameSize;

		//
		// SMRAM range check already covered before
		// Data buffer should not contain SMM range
		//
		if (InfoSize > CommBufferPayloadSize) {
			DPRINTF("SetVariable: Data size exceed communication "
				"buffer size limit!\n");
			Status = EFI_ACCESS_DENIED;
			break;
		}

		//
		// The VariableSpeculationBarrier() call here is to ensure the previous
		// range/content checks for the CommBuffer have been completed before the
		// subsequent consumption of the CommBuffer content.
		//
		//VariableSpeculationBarrier ();
		if ((SmmVariableHeader->NameSize < sizeof (CHAR16)) ||
		    (SmmVariableHeader->
		     Name[SmmVariableHeader->NameSize/sizeof (CHAR16) - 1] !=
		     L'\0')) {
			//
			// Make sure VariableName is A Null-terminated string.
			//
			DPRINTF("SetVariable: VariableName NOT Null-terminated!\n");
			Status = EFI_ACCESS_DENIED;
			break;
		}

		mm_memdump(SmmVariableFunctionHeader->Data, CommBufferPayloadSize, "SET");

{
	UINT8 Count = mVarCount;

	if (Count) {
		SMM_VARIABLE_COMMUNICATE_ACCESS_VARIABLE *pSet1, *pSet2;

		/* Check if same Vendor Guid and Variable Name pre-exists */
		while (Count) {
			if (CompareMem(SmmVariableFunctionHeader->Data,
				       &mVarBuffer[Count - 1], sizeof(EFI_GUID)) == 0) {
				pSet1 = (SMM_VARIABLE_COMMUNICATE_ACCESS_VARIABLE *)SmmVariableFunctionHeader->Data;
				pSet2 = (SMM_VARIABLE_COMMUNICATE_ACCESS_VARIABLE *)&mVarBuffer[Count - 1];

				if ((pSet1->NameSize == pSet2->NameSize) &&
				    (CompareMem (pSet1->Name, pSet2->Name,
						 pSet1->NameSize) == 0)) {
					/* Match found, update the existing data */
					DPRINTF("Ranbir: Update Existing Variable, At Count = %d\n", Count);
					//mm_memdump(&mVarBuffer[Count - 1], CommBufferPayloadSize, "UCACHE");
					CopyMem(&mVarBuffer[Count - 1], SmmVariableFunctionHeader->Data, CommBufferPayloadSize);
					//DPRINTF("Ranbir: Existing Variable - New Data\n");
					//mm_memdump(&mVarBuffer[Count - 1], CommBufferPayloadSize, "UCACHE");
					break;
				}
			}
			Count--;
		};
	}

	if (!Count) {
		CopyMem(&mVarBuffer[mVarCount], SmmVariableFunctionHeader->Data,
			CommBufferPayloadSize);
		mm_memdump(&mVarBuffer[mVarCount], CommBufferPayloadSize, "NCACHE");
		mVarCount++;
		DPRINTF("Ranbir: Added New Variable with Attribues = 0x%x, Net Count = %d\n", ((SMM_VARIABLE_COMMUNICATE_ACCESS_VARIABLE *)SmmVariableFunctionHeader->Data)->Attributes, mVarCount);
	}
}

		DPRINTF("Ranbir: SmmVariableHeader->Attributes = %d\n", SmmVariableHeader->Attributes);
		DPRINTF("Ranbir: SmmVariableHeader->NameSize = %lld\n", SmmVariableHeader->NameSize);
		DPRINTF("Ranbir: SmmVariableHeader->DataSize = %lld\n", SmmVariableHeader->DataSize);
		break;

	/*case SMM_VARIABLE_FUNCTION_GET_VARIABLE:
		memcpy(m_var_bfr_payload, SmmVariableFunctionHeader->Data,
		       CommBufferPayloadSize);
		status = fxn_get_variable(SmmVariableFunctionHeader->Data,
					  CommBufferPayloadSize);
		if (status == EFI_SUCCESS)
			memcpy(SmmVariableFunctionHeader->Data, m_var_bfr_payload,
			       CommBufferPayloadSize);

		break;

	case SMM_VARIABLE_FUNCTION_SET_VARIABLE:
		memcpy(m_var_bfr_payload, SmmVariableFunctionHeader->Data,
		       CommBufferPayloadSize);
		status = fxn_set_variable(SmmVariableFunctionHeader->Data,
					  CommBufferPayloadSize);
		if (status == EFI_SUCCESS)
			memcpy(SmmVariableFunctionHeader->Data, m_var_bfr_payload,
			       CommBufferPayloadSize);
		break;*/

		case SMM_VARIABLE_FUNCTION_QUERY_VARIABLE_INFO:
		case SMM_VARIABLE_FUNCTION_READY_TO_BOOT:
		case SMM_VARIABLE_FUNCTION_EXIT_BOOT_SERVICE:
		case SMM_VARIABLE_FUNCTION_GET_STATISTICS:
		case SMM_VARIABLE_FUNCTION_LOCK_VARIABLE:
		case SMM_VARIABLE_FUNCTION_VAR_CHECK_VARIABLE_PROPERTY_SET:
		case SMM_VARIABLE_FUNCTION_VAR_CHECK_VARIABLE_PROPERTY_GET:
		case SMM_VARIABLE_FUNCTION_INIT_RUNTIME_VARIABLE_CACHE_CONTEXT:
		case SMM_VARIABLE_FUNCTION_SYNC_RUNTIME_CACHE:
		case SMM_VARIABLE_FUNCTION_GET_RUNTIME_CACHE_INFO:
		default:
			DPRINTF("Ranbir: Processing SMM_VARIABLE_FUNCTION_TBS\n");
			break;
	}

	SmmVariableFunctionHeader->ReturnStatus = Status;

	DPRINTF("====================================================> "
		"%s: EXIT \n", __func__);

	return RPMI_SUCCESS;
}
