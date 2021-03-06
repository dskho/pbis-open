/* Editor Settings: expandtabs and use 4 spaces for indentation
 * ex: set softtabstop=4 tabstop=8 expandtab shiftwidth=4: *
 * -*- mode: c, c-basic-offset: 4 -*- */

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
 *        sighandler.c
 *
 * Abstract:
 *
 *        BeyondTrust Security and Authentication Subsystem (LSASS)
 * 
 *        Signal Handler API
 *
 * Authors: Krishna Ganugapati (krishnag@likewisesoftware.com)
 *          Sriram Nambakam (snambakam@likewisesoftware.com)
 *          Kyle Stemen (kstemen@likewisesoftware.com)
 */

#include "lsassd.h"

static
void
LsaSrvNOPHandler(
    int unused
    )
{
}

DWORD
LsaSrvIgnoreSIGHUP(
    VOID
    )
{
    DWORD dwError = LW_ERROR_SUCCESS;

    // Instead of ignoring the signal by passing SIG_IGN, we install a nop
    // signal handler. This way if we later decide to catch it with sigwait,
    // the signal will still get delivered to the process.
    if (signal(SIGHUP, LsaSrvNOPHandler) < 0) {
        dwError = LwMapErrnoToLwError(errno);
        BAIL_ON_LSA_ERROR(dwError);
    }

cleanup:

    return dwError;

error:

    goto cleanup;
}

static
void
LsaSrvInterruptHandler(
    int Signal
    )
{
    if (Signal == SIGINT)
    {
        raise(SIGTERM);
    }
}

static
void
LsaSrvHandleSIGHUP(
    VOID
    )
{
    DWORD dwError = 0;
    HANDLE hServer = (HANDLE)NULL;

    LSA_LOG_VERBOSE("Refreshing configuration");

    dwError = LsaSrvOpenServer(
                getuid(),
                getgid(),
                getpid(),
                &hServer);
    BAIL_ON_LSA_ERROR(dwError);

    dwError = LsaSrvRefreshConfiguration(hServer);
    BAIL_ON_LSA_ERROR(dwError); 

    LSA_LOG_INFO("Refreshed configuration successfully");

cleanup:

    if (hServer != (HANDLE)NULL)
    {
        LsaSrvCloseServer(hServer);
    }

    return;

error:

    LSA_LOG_ERROR("Failed to refresh configuration. [Error code:%u]", dwError);

    goto cleanup;
}

DWORD
LsaSrvHandleSignals(
    VOID
    )
{
    DWORD dwError = 0;
    struct sigaction action;
    sigset_t catch_signal_mask;
    sigset_t old_signal_mask;
    int which_signal = 0;
    BOOLEAN bWaitForSignals = TRUE;
    int sysRet = 0;

    // After starting up threads, we now want to handle SIGINT async
    // instead of using sigwait() on it.  The reason for this is so
    // that a debugger (such as gdb) can break in properly.
    // See http://sourceware.org/ml/gdb/2007-03/msg00145.html and
    // http://bugzilla.kernel.org/show_bug.cgi?id=9039.

    memset(&action, 0, sizeof(action));
    action.sa_handler = LsaSrvInterruptHandler;

    sysRet = sigaction(SIGINT, &action, NULL);
    dwError = (sysRet != 0) ? LwMapErrnoToLwError(errno) : 0;
    BAIL_ON_LSA_ERROR(dwError);

    // Unblock SIGINT
    sigemptyset(&catch_signal_mask);
    sigaddset(&catch_signal_mask, SIGINT);

    dwError = pthread_sigmask(SIG_UNBLOCK, &catch_signal_mask, NULL);
    BAIL_ON_LSA_ERROR(dwError);

    // These should already be blocked...
    sigemptyset(&catch_signal_mask);
    sigaddset(&catch_signal_mask, SIGTERM);
    sigaddset(&catch_signal_mask, SIGHUP);
    sigaddset(&catch_signal_mask, SIGQUIT);
    sigaddset(&catch_signal_mask, SIGPIPE);

    dwError = pthread_sigmask(SIG_BLOCK, &catch_signal_mask, &old_signal_mask);
    BAIL_ON_LSA_ERROR(dwError);

    while (bWaitForSignals)
    {
      
      /* wait for a signal to arrive */
      sigwait(&catch_signal_mask, &which_signal);

      LSA_LOG_WARNING("Received signal [%d]", which_signal);
      
      switch (which_signal)
        {
          
        case SIGINT:
        case SIGTERM:
        case SIGQUIT:
          
          {
            LsaSrvStopProcess();

            bWaitForSignals = FALSE;
            
            break;
          }
      
        case SIGPIPE:

          {
             LSA_LOG_DEBUG("Handled SIGPIPE");

             break;
          }

        case SIGHUP:

          {
             LsaSrvHandleSIGHUP();

             break;
          }

        }
    }

error:
    return dwError;
}

DWORD
LsaSrvStopProcess(
    VOID
    )
{
    LsaSrvSetProcessToExit(TRUE);

    return 0;
}
