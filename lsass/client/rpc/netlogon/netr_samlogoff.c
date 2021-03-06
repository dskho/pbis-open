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
 *        netr_samlogoff.c
 *
 * Abstract:
 *
 *        Remote Procedure Call (RPC) Client Interface
 *
 *        NetrSamLogoff functions.
 *
 * Authors: Rafal Szczesniak (rafal@likewise.com)
 *          Gerald Carter (gcarter@likewise.com)
 */

#include "includes.h"


NTSTATUS
NetrSamLogoff(
    IN  NETR_BINDING      hBinding,
    IN  NetrCredentials  *pCreds,
    IN  PCWSTR            pwszServer,
    IN  PCWSTR            pwszDomain,
    IN  PCWSTR            pwszComputer,
    IN  PCWSTR            pwszUsername,
    IN  PCWSTR            pwszPassword,
    IN  UINT32            LogonLevel
    )
{
    NTSTATUS ntStatus = STATUS_SUCCESS;
    DWORD dwError = ERROR_SUCCESS;
    PWSTR pwszServerName = NULL;
    PWSTR pwszComputerName = NULL;
    NetrAuth *pAuth = NULL;
    NetrAuth *pReturnedAuth = NULL;
    NetrLogonInfo LogonInfo = {0};
    NetrPasswordInfo *pPassInfo = NULL;
    DWORD dwOffset = 0;
    DWORD dwSpaceLeft = 0;
    DWORD dwSize = 0;

    BAIL_ON_INVALID_PTR(hBinding, ntStatus);
    BAIL_ON_INVALID_PTR(pCreds, ntStatus);
    BAIL_ON_INVALID_PTR(pwszServer, ntStatus);
    /* pwszDomain can be NULL */
    BAIL_ON_INVALID_PTR(pwszComputer, ntStatus);
    BAIL_ON_INVALID_PTR(pwszUsername, ntStatus);
    BAIL_ON_INVALID_PTR(pwszPassword, ntStatus);

    if (!(LogonLevel == 1 ||
          LogonLevel == 3 ||
          LogonLevel == 5))
    {
        ntStatus = STATUS_INVALID_INFO_CLASS;
        BAIL_ON_NT_STATUS(ntStatus);
    }

    dwError = LwAllocateWc16String(&pwszServerName,
                                   pwszServer);
    BAIL_ON_WIN_ERROR(dwError);

    dwError = LwAllocateWc16String(&pwszComputerName,
                                   pwszComputer);
    BAIL_ON_WIN_ERROR(dwError);

    /* Create authenticator info with credentials chain */
    ntStatus = NetrAllocateMemory(OUT_PPVOID(&pAuth),
                                  sizeof(NetrAuth));
    BAIL_ON_NT_STATUS(ntStatus);

    pCreds->sequence += 2;
    NetrCredentialsCliStep(pCreds);

    pAuth->timestamp = pCreds->sequence;
    memcpy(pAuth->cred.data,
           pCreds->cli_chal.data,
           sizeof(pAuth->cred.data));

    /* Allocate returned authenticator */
    ntStatus = NetrAllocateMemory(OUT_PPVOID(&pReturnedAuth),
                                  sizeof(NetrAuth));
    BAIL_ON_NT_STATUS(ntStatus);

    ntStatus = NetrAllocateLogonPasswordInfo(NULL,
                                             &dwOffset,
                                             NULL,
                                             pwszDomain,
                                             pwszComputerName,
                                             pwszUsername,
                                             pwszPassword,
                                             pCreds,
                                             &dwSize);
    BAIL_ON_NT_STATUS(ntStatus);

    dwSpaceLeft = dwSize;
    dwSize      = 0;
    dwOffset    = 0;

    ntStatus = NetrAllocateMemory(OUT_PPVOID(&pPassInfo),
                                  dwSpaceLeft);
    BAIL_ON_NT_STATUS(ntStatus);

    ntStatus = NetrAllocateLogonPasswordInfo(pPassInfo,
                                             &dwOffset,
                                             &dwSpaceLeft,
                                             pwszDomain,
                                             pwszComputerName,
                                             pwszUsername,
                                             pwszPassword,
                                             pCreds,
                                             &dwSize);
    BAIL_ON_NT_STATUS(ntStatus);

    switch (LogonLevel)
    {
    case 1:
        LogonInfo.password1 = pPassInfo;
        break;

    case 3:
        LogonInfo.password3 = pPassInfo;
        break;

    case 5:
        LogonInfo.password5 = pPassInfo;
        break;
    }

    DCERPC_CALL(ntStatus, cli_NetrLogonSamLogoff(hBinding,
                                                 pwszServerName,
                                                 pwszComputerName,
                                                 pAuth,
                                                 pReturnedAuth,
                                                 LogonLevel,
                                                 &LogonInfo));
    BAIL_ON_NT_STATUS(ntStatus);

cleanup:
    LW_SAFE_FREE_MEMORY(pwszServerName);
    LW_SAFE_FREE_MEMORY(pwszComputerName);

    if (pAuth)
    {
        NetrFreeMemory(pAuth);
    }

    if (pReturnedAuth)
    {
        NetrFreeMemory(pReturnedAuth);
    }

    if (pPassInfo)
    {
        NetrFreeMemory(pPassInfo);
    }

    if (ntStatus == STATUS_SUCCESS &&
        dwError != ERROR_SUCCESS)
    {
        ntStatus = LwWin32ErrorToNtStatus(dwError);
    }

    return ntStatus;

error:
    goto cleanup;
}
