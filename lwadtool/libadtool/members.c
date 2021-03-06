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
 * Module Name:
 *
 *        members.c
 *
 * Abstract:
 *
 *        Methods for AD group membeship management.
 *
 * Authors: Author: CORP\slavam
 * 
 * Created on: Mar 17, 2010
 *
 */

#include "includes.h"

/**
 * Shared add/remove to/from group validate method.
 * @param action Action reference.
 * @param isRemove TRUE is this is RemoveFromGroup validate method; FALSE otherwise.
 */
static DWORD
ValidateAdtAddToRemoveFromGroupAction(IN AdtActionTP action, IN BOOL isRemove)
{
    DWORD dwError = 0;
    AppContextTP appContext = (AppContextTP) ((AdtActionBaseTP) action)->opaque;

    if (!action->addToGroup.targetGroup) {
        dwError = isRemove
                ? ADT_ERR_ARG_MISSING_FROM_GROUP
                : ADT_ERR_ARG_MISSING_TO_GROUP;
        ADT_BAIL_ON_ERROR_NP(dwError);
    }

    SwitchToSearchConnection(action);

    if (!action->addToGroup.user && !action->addToGroup.group) {
        dwError = ADT_ERR_ARG_MISSING_USER_GROUP;
        ADT_BAIL_ON_ERROR_NP(dwError);
    }

    if (action->addToGroup.user && action->addToGroup.group) {
        dwError = ADT_ERR_INVALID_USER_GROUP;
        ADT_BAIL_ON_ERROR_NP(dwError);
    }

    if(action->addToGroup.user) {
        dwError = ProcessDash(&(action->addToGroup.user));
        ADT_BAIL_ON_ERROR_NP(dwError);

        dwError = OpenADSearchConnectionDomain(action, &(action->addToGroup.user));
        ADT_BAIL_ON_ERROR_NP(dwError);

        SwitchToMatchingConnection(action, &(action->addToGroup.user));

        dwError = LocateADUser(appContext, &(action->addToGroup.user));
        ADT_BAIL_ON_ERROR_NP(dwError);
    }
    else {
        dwError = ProcessDash(&(action->addToGroup.group));
        ADT_BAIL_ON_ERROR_NP(dwError);

        dwError = OpenADSearchConnectionDomain(action, &(action->addToGroup.group));
        ADT_BAIL_ON_ERROR_NP(dwError);

        SwitchToMatchingConnection(action, &(action->addToGroup.group));

        dwError = LocateADGroup(appContext, &(action->addToGroup.group));
        ADT_BAIL_ON_ERROR_NP(dwError);
    }

    dwError = ProcessDash(&(action->addToGroup.targetGroup));
    ADT_BAIL_ON_ERROR_NP(dwError);

    SwitchToMatchingConnection(action, &(action->addToGroup.targetGroup));

    dwError = LocateADGroup(appContext, &(action->addToGroup.targetGroup));
    ADT_BAIL_ON_ERROR_NP(dwError);

    cleanup:
        return dwError;

    error:
        goto cleanup;
}

static DWORD
ValidateGroupType(IN AdtActionTP action)
{
    DWORD dwError = 0;
    AppContextTP appContext = (AppContextTP) ((AdtActionBaseTP) action)->opaque;
    INT i, j;
    AttrValsT *avp = NULL;

    dwError = LwAllocateMemory(2 * sizeof(AttrValsT), OUT_PPVOID(&avp));
    ADT_BAIL_ON_ALLOC_FAILURE(!dwError);

    avp[0].attr = "groupType";

    dwError = GetObjectAttrs(appContext, action->addToGroup.targetGroup, avp);
    ADT_BAIL_ON_ERROR_NP(dwError);

    if(IsEqual(avp[0].vals[0], GROUP_TYPE_GLOBAL, 1)) {
        dwError = ADT_ERR_FAILED_GTYPE_CHECK;
        ADT_BAIL_ON_ERROR_NP(dwError);
    }

    cleanup:
        if (avp) {
            for (i = 0; avp[i].vals; ++i) {
                for (j = 0; avp[i].vals[j]; ++j) {
                    LW_SAFE_FREE_MEMORY(avp[i].vals[j]);
                }

                LW_SAFE_FREE_MEMORY(avp[i].vals);
            }

            LW_SAFE_FREE_MEMORY(avp);
        }

        return dwError;

    error:
        goto cleanup;
}

/**
 * Shared add/remove to/from group execute method.
 * @param action Action reference.
 * @param isRemove TRUE is this is RemoveFromGroup execute method; FALSE otherwise.
 */
static DWORD
ExecuteAdtAddToRemoveFromGroupAction(IN AdtActionTP action, IN BOOL isRemove)
{
    DWORD dwError = 0;
    AppContextTP appContext = (AppContextTP) ((AdtActionBaseTP) action)->opaque;
    INT i = 0;
    INT j = 0;
    AttrValsT *avpGrp = NULL;
    PSTR member = NULL;
    PSTR memberForeign = NULL;
    PSTR sid = NULL;

    if(action->addToGroup.user) {
        member = action->addToGroup.user;
    }
    else {
        member = action->addToGroup.group;
    }

    dwError = LwAllocateMemory(2 * sizeof(AttrValsT), OUT_PPVOID(&avpGrp));
    ADT_BAIL_ON_ALLOC_FAILURE(!dwError);

    dwError = LwAllocateMemory(2 * sizeof(PSTR), OUT_PPVOID(&(avpGrp[0].vals)));
    ADT_BAIL_ON_ALLOC_FAILURE(!dwError);

    avpGrp[0].attr = "member";
    if(IsMultiForestMode(action))
    {
        SwitchConnection(action);
        
        dwError = GetObjectSID(appContext, action->addToGroup.user, &sid);
        ADT_BAIL_ON_ERROR_NP(dwError);

        SwitchConnection(action);
        
        dwError = LwAllocateStringPrintf(&memberForeign,"<SID=%s>",sid);
        ADT_BAIL_ON_ALLOC_FAILURE_NP(!dwError);

        avpGrp[0].vals[0] = memberForeign;
    }
    else
    {
        avpGrp[0].vals[0] = member;
    }

    PrintStderr(appContext,
                LogLevelVerbose,
                "%s: Changing membership of member %s in group %s ...\n",
                appContext->actionName,
                member, action->addToGroup.targetGroup);

    if(isRemove) {
        dwError = ModifyADObject(appContext, action->addToGroup.targetGroup, avpGrp, 1);
    }
    else {
        if(
           appContext->modifyConn.conn && 
           appContext->searchConn.conn &&
           !IsEqual(appContext->searchConn.defaultNC, appContext->modifyConn.defaultNC, 1)
        ) {
           dwError = ValidateGroupType(action);
           ADT_BAIL_ON_ERROR_NP(dwError);
        }

        dwError = ModifyADObject(appContext, action->addToGroup.targetGroup, avpGrp, 0);
    }
    ADT_BAIL_ON_ERROR_NP(dwError);

    PrintStderr(appContext,
                LogLevelVerbose,
                "%s: Done changing membership of member %s\n",
                appContext->actionName,
                member);

    if(appContext->gopts.isPrintDN) {
        if(!appContext->gopts.isQuiet) {
            PrintResult(appContext, LogLevelNone, "%s\n", action->addToGroup.targetGroup);
        }
    }
    else {
        if (!appContext->gopts.isQuiet) {
            PrintResult(appContext,
                        LogLevelNone,
                        "Member %s has been %s group %s\n",
                        member, isRemove ? "removed from" : "added to", action->addToGroup.targetGroup);
        }
    }

    cleanup:
        if (avpGrp) {
            for (i = 0; avpGrp[i].vals; ++i) {
                for (j = 0; avpGrp[i].vals[j]; ++j) {
                    LW_SAFE_FREE_MEMORY(avpGrp[i].vals[j]);
                }

                LW_SAFE_FREE_MEMORY(avpGrp[i].vals);
            }

            LW_SAFE_FREE_MEMORY(avpGrp);
        }

        return dwError;

    error:
        goto cleanup;
}

/**
 * Shared add/remove to/from local group execute method.
 * @param action Action reference.
 * @param isRemove TRUE is this is RemoveFromGroup execute method; FALSE otherwise.
 */
DWORD
ExecuteAdtAddToRemoveFromGroupActionLocal(IN AdtActionTP action, IN BOOL isRemove)
{
    DWORD dwError = 0;
    AppContextTP appContext = (AppContextTP) ((AdtActionBaseTP) action)->opaque;
    INT i = 0;
    INT j = 0;
    AttrValsT *avp = NULL;
    AttrValsT *avpGrp = NULL;
    PSTR member = NULL;
    PSTR group = NULL;

    /** Get samAccountName of the member */
    dwError = LwAllocateMemory(2 * sizeof(AttrValsT), OUT_PPVOID(&avp));
    ADT_BAIL_ON_ALLOC_FAILURE(!dwError);

    avp[0].attr = "samAccountName";

    dwError = GetObjectAttrs(appContext, action->addToGroup.user, avp);
    ADT_BAIL_ON_ERROR_NP(dwError);

    if(!avp[0].vals || !avp[0].vals[0]) {
        dwError = ADT_ERR_FAILED_AD_GET_ATTR;
        ADT_BAIL_ON_ERROR_NP(dwError);
    }

    dwError = LwStrDupOrNull((PCSTR) avp[0].vals[0], &member);
    ADT_BAIL_ON_ALLOC_FAILURE_NP(!dwError);

    /** Get samAccountName of the group */
    dwError = LwAllocateMemory(2 * sizeof(AttrValsT), OUT_PPVOID(&avpGrp));
    ADT_BAIL_ON_ALLOC_FAILURE(!dwError);

    avpGrp[0].attr = "samAccountName";

    SwitchToModifyConnection(action);

    dwError = GetObjectAttrs(appContext, action->addToGroup.targetGroup, avpGrp);
    ADT_BAIL_ON_ERROR_NP(dwError);

    if(!avpGrp[0].vals || !avpGrp[0].vals[0]) {
        dwError = ADT_ERR_FAILED_AD_GET_ATTR;
        ADT_BAIL_ON_ERROR_NP(dwError);
    }

    dwError = LwStrDupOrNull((PCSTR) avpGrp[0].vals[0], &group);
    ADT_BAIL_ON_ALLOC_FAILURE_NP(!dwError);

    PrintStderr(appContext,
                LogLevelVerbose,
                "%s: Changing membership of member %s in group %s ...\n",
                appContext->actionName,
                member, group);

    dwError = isRemove
            ? AdtNetLocalGroupDeleteMember(appContext, group, member)
            : AdtNetLocalGroupAddMember(appContext, group, member);
    ADT_BAIL_ON_ERROR_NP(dwError);

    PrintStderr(appContext,
                LogLevelVerbose,
                "%s: Done changing membership of member %s\n",
                appContext->actionName,
                member);

    if(appContext->gopts.isPrintDN) {
        if(!appContext->gopts.isQuiet) {
            PrintResult(appContext, LogLevelNone, "%s\n", action->addToGroup.targetGroup);
        }
    }
    else {
        if (!appContext->gopts.isQuiet) {
            PrintResult(appContext,
                        LogLevelNone,
                        "Member %s has been %s group %s\n",
                        member, isRemove ? "removed from" : "added to", group);
        }
    }

    cleanup:
        if (avp) {
            for (i = 0; avp[i].vals; ++i) {
                for (j = 0; avp[i].vals[j]; ++j) {
                    LW_SAFE_FREE_MEMORY(avp[i].vals[j]);
                }

                LW_SAFE_FREE_MEMORY(avp[i].vals);
            }

            LW_SAFE_FREE_MEMORY(avp);
        }

        if (avpGrp) {
            for (i = 0; avpGrp[i].vals; ++i) {
                for (j = 0; avpGrp[i].vals[j]; ++j) {
                    LW_SAFE_FREE_MEMORY(avpGrp[i].vals[j]);
                }

                LW_SAFE_FREE_MEMORY(avpGrp[i].vals);
            }

            LW_SAFE_FREE_MEMORY(avpGrp);
        }

        return dwError;

    error:
        goto cleanup;
}

/********************************************************************/
/*                         Add to group                             */
/********************************************************************/

/**
 * Action initialization method.
 */
DWORD InitAdtAddToGroupAction(IN AdtActionTP action)
{
    return InitBaseActionWithNetAPI(action);
}

/**
 * Action validate method.
 */
DWORD ValidateAdtAddToGroupAction(IN AdtActionTP action)
{
    return ValidateAdtAddToRemoveFromGroupAction(action, FALSE);
}

/**
 * Action execute method.
 */
DWORD ExecuteAdtAddToGroupAction(IN AdtActionTP action)
{
    return ExecuteAdtAddToRemoveFromGroupAction(action, FALSE);
}

/**
 * Action clean up method.
 */
DWORD CleanUpAdtAddToGroupAction(IN AdtActionTP action)
{
    return CleanUpBaseWithNetAPIAction(action);
}

/********************************************************************/
/*                      Remove from group                           */
/********************************************************************/

DWORD InitAdtRemoveFromGroupAction(IN AdtActionTP action)
{
    return InitBaseActionWithNetAPI(action);
}

DWORD ValidateAdtRemoveFromGroupAction(IN AdtActionTP action)
{
    return ValidateAdtAddToRemoveFromGroupAction(action, TRUE);
}

DWORD ExecuteAdtRemoveFromGroupAction(IN AdtActionTP action)
{
    return ExecuteAdtAddToRemoveFromGroupAction(action, TRUE);
}

DWORD CleanUpAdtRemoveFromGroupAction(IN AdtActionTP action)
{
    return CleanUpBaseWithNetAPIAction(action);
}
