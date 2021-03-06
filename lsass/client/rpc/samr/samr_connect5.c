/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil; tab-width: 4 -*-
 * ex: set softtabstop=4 tabstop=8 expandtab shiftwidth=4: *
 * Editor Settings: expandtabs and use 4 spaces for indentation */

/*
 * Copyright © BeyondTrust Software 2004 - 2019
 * All rights reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *        http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * BEYONDTRUST MAKES THIS SOFTWARE AVAILABLE UNDER OTHER LICENSING TERMS AS
 * WELL. IF YOU HAVE ENTERED INTO A SEPARATE LICENSE AGREEMENT WITH
 * BEYONDTRUST, THEN YOU MAY ELECT TO USE THE SOFTWARE UNDER THE TERMS OF THAT
 * SOFTWARE LICENSE AGREEMENT INSTEAD OF THE TERMS OF THE APACHE LICENSE,
 * NOTWITHSTANDING THE ABOVE NOTICE.  IF YOU HAVE QUESTIONS, OR WISH TO REQUEST
 * A COPY OF THE ALTERNATE LICENSING TERMS OFFERED BY BEYONDTRUST, PLEASE CONTACT
 * BEYONDTRUST AT beyondtrust.com/contact
 */

/*
 * Copyright (C) BeyondTrust Software. All rights reserved.
 *
 * Module Name:
 *
 *        samr_connect5.c
 *
 * Abstract:
 *
 *        Remote Procedure Call (RPC) Client Interface
 *
 *        SamrConnect5 function
 *
 * Authors: Rafal Szczesniak (rafal@likewise.com)
 */

#include "includes.h"


NTSTATUS
SamrConnect5(
    IN  SAMR_BINDING     hBinding,
    IN  PCWSTR           pwszSysName,
    IN  UINT32           AccessMask,
    IN  UINT32           LevelIn,
    IN  SamrConnectInfo *pInfoIn,
    IN  PUINT32          pLevelOut,
    OUT SamrConnectInfo *pInfoOut,
    OUT CONNECT_HANDLE  *phConn
    )
{
    NTSTATUS ntStatus = STATUS_SUCCESS;
    DWORD dwError = ERROR_SUCCESS;
    WCHAR wszDefaultSysName[] = SAMR_DEFAULT_SYSNAME;
    PWSTR pwszSystemName = NULL;
    CONNECT_HANDLE hConn = NULL;
    UINT32 Level = 0;
    SamrConnectInfo Info;

    BAIL_ON_INVALID_PTR(hBinding, ntStatus);
    BAIL_ON_INVALID_PTR(pInfoIn, ntStatus);
    BAIL_ON_INVALID_PTR(pLevelOut, ntStatus);
    BAIL_ON_INVALID_PTR(pInfoOut, ntStatus);
    BAIL_ON_INVALID_PTR(phConn, ntStatus);

    memset(&Info, 0, sizeof(Info));

    dwError = LwAllocateWc16String(
                        &pwszSystemName,
                        (pwszSysName) ? pwszSysName : &(wszDefaultSysName[0]));
    BAIL_ON_WIN_ERROR(dwError);

    DCERPC_CALL(ntStatus, cli_SamrConnect5((handle_t)hBinding,
                                           pwszSystemName,
                                           AccessMask,
                                           LevelIn,
                                           pInfoIn,
                                           &Level,
                                           &Info,
                                           &hConn));
    BAIL_ON_NT_STATUS(ntStatus);

    *pLevelOut = Level;
    *pInfoOut  = Info;
    *phConn    = hConn;

cleanup:
    LW_SAFE_FREE_MEMORY(pwszSystemName);

    if (ntStatus == STATUS_SUCCESS &&
        dwError != ERROR_SUCCESS)
    {
        ntStatus = LwWin32ErrorToNtStatus(dwError);
    }

    return ntStatus;

error:
    if (pInfoOut)
    {
        memset(pInfoOut, 0, sizeof(*pInfoOut));
    }

    if (phConn)
    {
        *phConn = NULL;
    }

    if (pLevelOut)
    {
        *pLevelOut = 0;
    }

    goto cleanup;
}
