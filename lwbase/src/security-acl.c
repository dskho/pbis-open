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
 *        security-acl.c
 *
 * Abstract:
 *
 *        ACE/ACL Functions in Security Module.
 *
 * Authors: Danilo Almeida (dalmeida@likewise.com)
 *
 */

#include "security-includes.h"

//
// ACE Functions (API2)
//

inline
static
BOOLEAN
RtlpValidAceHeader(
    IN PACE_HEADER Ace
    )
{
    return (Ace &&
            (Ace->AceSize >= sizeof(ACE_HEADER)) &&
            LW_IS_VALID_FLAGS(Ace->AceFlags, VALID_ACE_FLAGS_MASK));
}

static
BOOLEAN
RtlpValidAccessAllowedAce(
    IN USHORT AceSize,
    IN PSID Sid,
    OUT OPTIONAL PNTSTATUS Status
    )
{
    NTSTATUS status = STATUS_SUCCESS;
    USHORT sizeRequired = 0;

    sizeRequired = RtlLengthAccessAllowedAce(Sid);
    if (sizeRequired != AceSize)
    {
        status = STATUS_INVALID_ACL;
        GOTO_CLEANUP();
    }

    if (!RtlValidSid(Sid))
    {
        status = STATUS_INVALID_SID;
        GOTO_CLEANUP();
    }

    status = STATUS_SUCCESS;

cleanup:
    if (Status)
    {
        *Status = status;
    }

    return NT_SUCCESS(status) ? TRUE : FALSE;
}
    
static
BOOLEAN
RtlpValidAccessAllowedObjectAce(
    IN USHORT AceSize,
    IN ULONG AceFlags,
    IN PSID Sid,
    OUT OPTIONAL PNTSTATUS Status
    )
{
    NTSTATUS status = STATUS_SUCCESS;
    USHORT sizeRequired = 0;

    sizeRequired = RtlLengthAccessAllowedObjectAce(Sid, AceFlags);
    if (sizeRequired != AceSize)
    {
        status = STATUS_INVALID_ACL;
        GOTO_CLEANUP();
    }

    if (!RtlValidSid(Sid))
    {
        status = STATUS_INVALID_SID;
        GOTO_CLEANUP();
    }

    status = STATUS_SUCCESS;

cleanup:
    if (Status)
    {
        *Status = status;
    }

    return NT_SUCCESS(status) ? TRUE : FALSE;
}
    
static
NTSTATUS
RtlpVerifyAceEx(
    IN PACE_HEADER Ace,
    IN BOOLEAN VerifyAceType
    )
{
    NTSTATUS status = STATUS_SUCCESS;
    ACCESS_MASK validMask = 0;

    if (!RtlpValidAceHeader(Ace))
    {
        status = STATUS_INVALID_ACL;
        GOTO_CLEANUP();
    }

    if (!VerifyAceType)
    {
        status = STATUS_SUCCESS;
        GOTO_CLEANUP();
    }

    // TODO - Perhaps support additional ACE types?

    switch (Ace->AceType)
    {
        case ACCESS_ALLOWED_ACE_TYPE:
        case ACCESS_DENIED_ACE_TYPE:
            validMask = VALID_DACL_ACCESS_MASK;
            break;
        case SYSTEM_AUDIT_ACE_TYPE:
            validMask = VALID_SACL_ACCESS_MASK;
            break;
        case ACCESS_ALLOWED_OBJECT_ACE_TYPE:
        case ACCESS_DENIED_OBJECT_ACE_TYPE:
            validMask = VALID_DACL_ACCESS_MASK;
            break;
        case SYSTEM_AUDIT_OBJECT_ACE_TYPE:
            validMask = VALID_SACL_ACCESS_MASK;
            break;
    }

    switch (Ace->AceType)
    {
        case ACCESS_ALLOWED_ACE_TYPE:
        case ACCESS_DENIED_ACE_TYPE:
        case SYSTEM_AUDIT_ACE_TYPE:
        {
            // These are all isomorphic.
            PACCESS_ALLOWED_ACE allowAce = (PACCESS_ALLOWED_ACE) Ace;
            if (!LW_IS_VALID_FLAGS(allowAce->Mask, validMask))
            {
                status = STATUS_INVALID_ACL;
                GOTO_CLEANUP();
            }
            RtlpValidAccessAllowedAce(
                    Ace->AceSize,
                    RtlpGetSidAccessAllowedAce(allowAce),
                    &status);
            GOTO_CLEANUP_ON_STATUS(status);
            break;
        }
        case ACCESS_ALLOWED_OBJECT_ACE_TYPE:
        case ACCESS_DENIED_OBJECT_ACE_TYPE:
        case SYSTEM_AUDIT_OBJECT_ACE_TYPE:
        {
            // These are all isomorphic.
            PACCESS_ALLOWED_OBJECT_ACE allowAce = (PACCESS_ALLOWED_OBJECT_ACE) Ace;
            if (!LW_IS_VALID_FLAGS(allowAce->Mask, validMask))
            {
                status = STATUS_INVALID_ACL;
                GOTO_CLEANUP();
            }
            RtlpValidAccessAllowedObjectAce(
                    Ace->AceSize,
                    allowAce->Flags,
                    RtlpGetSidAccessAllowedObjectAce(allowAce),
                    &status);
            GOTO_CLEANUP_ON_STATUS(status);
            break;
        }
        default:
            status = STATUS_INVALID_ACL;
            GOTO_CLEANUP();
    }

    status = STATUS_SUCCESS;

cleanup:
    return status;
}

inline
static
BOOLEAN
RtlpValidAce(
    IN PACE_HEADER Ace
    )
{
    return NT_SUCCESS(RtlpVerifyAceEx(Ace, FALSE));
}
    
USHORT
RtlLengthAccessAllowedAce(
    IN PSID Sid
    )
{
    return (LW_FIELD_OFFSET(ACCESS_ALLOWED_ACE, SidStart) +
            RtlLengthSid(Sid));
}

USHORT
RtlLengthAccessAllowedObjectAce(
    IN PSID Sid,
    IN ULONG AceFlags
    )
{
    USHORT Size = LW_FIELD_OFFSET(ACCESS_ALLOWED_OBJECT_ACE, ObjectType);
    
    switch (AceFlags) {
        case 0:
            break;
        case ACE_OBJECT_TYPE_PRESENT:
        case ACE_INHERITED_OBJECT_TYPE_PRESENT:
            Size += sizeof(GUID);
            break;
        default:
            Size += 2 * sizeof(GUID);
            break;
    }
    
    Size += RtlLengthSid(Sid);
    
    return ( Size );
}

USHORT
RtlLengthAccessDeniedAce(
    IN PSID Sid
    )
{
    return (LW_FIELD_OFFSET(ACCESS_DENIED_ACE, SidStart) +
            RtlLengthSid(Sid));
}

USHORT
RtlLengthSystemAuditAce(
    IN PSID Sid
    )
{
    return (LW_FIELD_OFFSET(SYSTEM_AUDIT_ACE, SidStart) +
            RtlLengthSid(Sid));
}

USHORT
RtlLengthAccessAuditAce(
    IN PSID Sid
    )
{
    return (LW_FIELD_OFFSET(SYSTEM_AUDIT_ACE, SidStart) +
            RtlLengthSid(Sid));
}

NTSTATUS
RtlInitializeAccessAllowedAce(
    OUT PACCESS_ALLOWED_ACE Ace,
    IN ULONG AceLength,
    IN USHORT AceFlags,
    IN ACCESS_MASK AccessMask,
    IN PSID Sid
    )
{
    NTSTATUS status = STATUS_SUCCESS;
    USHORT size = RtlLengthAccessAllowedAce(Sid);

    if (!LW_IS_VALID_FLAGS(AceFlags, VALID_ACE_FLAGS_MASK) ||
        // !LW_IS_VALID_FLAGS(AccessMask, VALID_DACL_ACCESS_MASK) ||
        !RtlValidSid(Sid))
    {
        status = STATUS_INVALID_PARAMETER;
        GOTO_CLEANUP();
    }

    if (AceLength < size)
    {
        status = STATUS_BUFFER_TOO_SMALL;
        GOTO_CLEANUP();
    }

    Ace->Header.AceType = ACCESS_ALLOWED_ACE_TYPE;
    Ace->Header.AceFlags = AceFlags;
    Ace->Header.AceSize = size;
    Ace->Mask = AccessMask;
    // We already know the size is sufficient
    RtlCopyMemory(&Ace->SidStart, Sid, RtlLengthSid(Sid));

cleanup:
    return status;
}

NTSTATUS
RtlInitializeAccessDeniedAce(
    OUT PACCESS_DENIED_ACE Ace,
    IN ULONG AceLength,
    IN USHORT AceFlags,
    IN ACCESS_MASK AccessMask,
    IN PSID Sid
    )
{
    NTSTATUS status = STATUS_SUCCESS;
    USHORT size = RtlLengthAccessDeniedAce(Sid);

    if (!LW_IS_VALID_FLAGS(AceFlags, VALID_ACE_FLAGS_MASK) ||
        !LW_IS_VALID_FLAGS(AccessMask, VALID_DACL_ACCESS_MASK) ||
        !RtlValidSid(Sid))
    {
        status = STATUS_INVALID_PARAMETER;
        GOTO_CLEANUP();
    }

    if (AceLength < size)
    {
        status = STATUS_BUFFER_TOO_SMALL;
        GOTO_CLEANUP();
    }

    Ace->Header.AceType = ACCESS_DENIED_ACE_TYPE;
    Ace->Header.AceFlags = AceFlags;
    Ace->Header.AceSize = size;
    Ace->Mask = AccessMask;
    // We already know the size is sufficient
    RtlCopyMemory(&Ace->SidStart, Sid, RtlLengthSid(Sid));

cleanup:
    return status;
}

NTSTATUS
RtlInitializeSystemAuditAce(
    OUT PSYSTEM_AUDIT_ACE Ace,
    IN ULONG AceLength,
    IN USHORT AceFlags,
    IN ACCESS_MASK AccessMask,
    IN PSID Sid
    )
{
    NTSTATUS status = STATUS_SUCCESS;
    USHORT size = RtlLengthSystemAuditAce(Sid);

    if (!LW_IS_VALID_FLAGS(AceFlags, VALID_ACE_FLAGS_MASK) ||
        !LW_IS_VALID_FLAGS(AccessMask, VALID_DACL_ACCESS_MASK) ||
        !RtlValidSid(Sid))
    {
        status = STATUS_INVALID_PARAMETER;
        GOTO_CLEANUP();
    }

    if (AceLength < size)
    {
        status = STATUS_BUFFER_TOO_SMALL;
        GOTO_CLEANUP();
    }

    Ace->Header.AceType = SYSTEM_AUDIT_ACE_TYPE;
    Ace->Header.AceFlags = AceFlags;
    Ace->Header.AceSize = size;
    Ace->Mask = AccessMask;
    // We already know the size is sufficient
    RtlCopyMemory(&Ace->SidStart, Sid, RtlLengthSid(Sid));

cleanup:
    return status;
}

//
// ACL Functions (API2)
//

inline
static
BOOLEAN
RtlpValidAclRevision(
    IN ULONG AclRevision
    )
{
    return ((AclRevision == ACL_REVISION) ||
            (AclRevision == ACL_REVISION_DS));
}

inline
static
BOOLEAN
RtlpValidAclHeader(
    IN PACL Acl
    )
{
    return (Acl &&
            (Acl->AclSize >= sizeof(ACL)) &&
            RtlpValidAclRevision(Acl->AclRevision) &&
            (Acl->Sbz1 == 0) &&
            (Acl->Sbz2 == 0) &&
            (Acl->AceCount <= ((((USHORT)-1) - sizeof(ACL)) / sizeof(ACE_HEADER))));
}

NTSTATUS
RtlInitializeAcl(
    OUT PACL Acl,
    OUT PUSHORT AclSizeUsed,
    IN ULONG AclLength,
    IN ULONG AclRevision
    )
{
    NTSTATUS status = STATUS_SUCCESS;

    if ((AclRevision != ACL_REVISION) &&
        (AclRevision != ACL_REVISION_DS))
    {
        status = STATUS_INVALID_PARAMETER;
        GOTO_CLEANUP();
    }

    if (AclLength > ((USHORT)-1))
    {
        status = STATUS_INVALID_PARAMETER;
        GOTO_CLEANUP();
    }

    if (AclLength < sizeof(ACL))
    {
        status = STATUS_BUFFER_TOO_SMALL;
        GOTO_CLEANUP();
    }

    Acl->AclRevision = AclRevision;
    Acl->AclSize = AclLength;
    Acl->AceCount = 0;
    Acl->Sbz1 = 0;
    Acl->Sbz2 = 0;

    // ISSUE-Do we need this?
    // Zero out memory in case this somehow gets serialized without
    // going through self-relative code.
    RtlZeroMemory(LW_PTR_ADD(Acl, sizeof(ACL)), AclLength - sizeof(ACL));

cleanup:
    *AclSizeUsed = sizeof(ACL);

    return status;
}

static
NTSTATUS
RtlpGetAceLocationFromOffset(
    IN PACL Acl,
    IN USHORT AclSizeUsed,
    IN USHORT AceOffset,
    OUT PVOID* AceLocation
    )
{
    NTSTATUS status = STATUS_SUCCESS;
    USHORT useOffset = 0;
    USHORT endOffset = 0;
    PVOID aceLocation = NULL;

    if (!RtlpValidAclHeader(Acl) ||
        (AclSizeUsed < sizeof(ACL)) ||
        (AclSizeUsed > Acl->AclSize))
    {
        status = STATUS_INVALID_ACL;
        GOTO_CLEANUP();
    }

    endOffset = AclSizeUsed - sizeof(ACL);
    if (AceOffset == ACL_END_ACE_OFFSET)
    {
        useOffset = endOffset;
    }
    else if (AceOffset >= endOffset)
    {
        status = STATUS_INVALID_PARAMETER;
        GOTO_CLEANUP();
    }
    else
    {
        useOffset = AceOffset;
    }

    aceLocation = LW_PTR_ADD(Acl, sizeof(ACL) + useOffset);

cleanup:
    if (!NT_SUCCESS(status))
    {
        aceLocation = NULL;
    }

    *AceLocation = aceLocation;

    return status;
}

inline
static
NTSTATUS
RtlpCheckEnoughAclBuffer(
    IN USHORT AclSize,
    IN USHORT AclSizeUsed,
    IN USHORT AceSize
    )
{
    NTSTATUS status = STATUS_SUCCESS;
    ULONG sizeNeeded = AclSizeUsed + AceSize;

    // Check for overflow.
    if (sizeNeeded > ((USHORT)-1))
    {
        // TODO-Perhaps use STATUS_INVALID_PARAMETER or some other code?
        status = STATUS_INTEGER_OVERFLOW;
        GOTO_CLEANUP();
    }
    
    if (AclSize < sizeNeeded)
    {
        status = STATUS_BUFFER_TOO_SMALL;
        GOTO_CLEANUP();
    }

cleanup:
    return status;
}

static
inline
NTSTATUS
RtlpMakeRoomForAceAtLocation(
    IN OUT PACL Acl,
    IN USHORT AclSizeUsed,
    IN PVOID AceLocation,
    IN USHORT AceSize
    )
{
    NTSTATUS status = STATUS_SUCCESS;
    PVOID aceEnd = NULL;
    PVOID aclEnd = NULL;

    status = RtlpCheckEnoughAclBuffer(Acl->AclSize, AclSizeUsed, AceSize);
    GOTO_CLEANUP_ON_STATUS(status);

    aceEnd = LW_PTR_ADD(AceLocation, AceSize);
    aclEnd = LW_PTR_ADD(Acl, AclSizeUsed);

    // Make enough room as needed
    RtlMoveMemory(aceEnd, AceLocation, LW_PTR_OFFSET(AceLocation, aclEnd));

cleanup:
    return status;
}

NTSTATUS
RtlInsertAce(
    IN OUT PACL Acl,
    IN OUT PUSHORT AclSizeUsed,
    IN USHORT AceOffset,
    IN PACE_HEADER Ace
    )
{
    NTSTATUS status = STATUS_SUCCESS;
    USHORT sizeUsed = *AclSizeUsed;
    PVOID aceLocation = NULL;

    if (!RtlpValidAce(Ace))
    {
        status = STATUS_INVALID_PARAMETER;
        GOTO_CLEANUP();
    }

    // TODO-Check ACE type vs ACL revision

    status = RtlpGetAceLocationFromOffset(
                    Acl,
                    sizeUsed,
                    AceOffset,
                    &aceLocation);
    GOTO_CLEANUP_ON_STATUS(status);

    status = RtlpMakeRoomForAceAtLocation(Acl, sizeUsed, aceLocation, Ace->AceSize);
    GOTO_CLEANUP_ON_STATUS(status);

    // Copy in the ACE
    RtlCopyMemory(aceLocation, Ace, Ace->AceSize);
    sizeUsed += Ace->AceSize;
    Acl->AceCount++;

cleanup:
    if (NT_SUCCESS(status))
    {
        *AclSizeUsed = sizeUsed;
    }

    return status;
}

NTSTATUS
RtlInsertAccessAllowedAce(
    IN OUT PACL Acl,
    IN OUT PUSHORT AclSizeUsed,
    IN USHORT AceOffset,
    IN USHORT AceFlags,
    IN ACCESS_MASK AccessMask,
    IN PSID Sid,
    OUT OPTIONAL PACCESS_ALLOWED_ACE* Ace
    )
{
    NTSTATUS status = STATUS_SUCCESS;
    USHORT sizeUsed = *AclSizeUsed;
    PACCESS_ALLOWED_ACE aceLocation = NULL;
    USHORT aceSize = 0;

    if (!RtlValidSid(Sid))
    {
        status = STATUS_INVALID_PARAMETER;
        GOTO_CLEANUP();
    }

    status = RtlpGetAceLocationFromOffset(
                    Acl,
                    sizeUsed,
                    AceOffset,
                    OUT_PPVOID(&aceLocation));
    GOTO_CLEANUP_ON_STATUS(status);

    aceSize = RtlLengthAccessAllowedAce(Sid);

    status = RtlpMakeRoomForAceAtLocation(Acl, sizeUsed, aceLocation, aceSize);
    GOTO_CLEANUP_ON_STATUS(status);

    // We know we have at least aceSize bytes available.
    status = RtlInitializeAccessAllowedAce(
                    aceLocation,
                    aceSize,
                    AceFlags,
                    AccessMask,
                    Sid);
    GOTO_CLEANUP_ON_STATUS(status);

    assert(aceSize == aceLocation->Header.AceSize);
    sizeUsed += aceSize;
    Acl->AceCount++;

cleanup:
    if (!NT_SUCCESS(status))
    {
        aceLocation = NULL;
    }
    else
    {
        *AclSizeUsed = sizeUsed;
    }

    if (Ace)
    {
        *Ace = aceLocation;
    }

    return status;
}

NTSTATUS
RtlInsertAccessDeniedAce(
    IN OUT PACL Acl,
    IN OUT PUSHORT AclSizeUsed,
    IN USHORT AceOffset,
    IN USHORT AceFlags,
    IN ACCESS_MASK AccessMask,
    IN PSID Sid,
    OUT OPTIONAL PACCESS_DENIED_ACE* Ace
    )
{
    NTSTATUS status = STATUS_SUCCESS;
    USHORT sizeUsed = *AclSizeUsed;
    PACCESS_DENIED_ACE aceLocation = NULL;
    USHORT aceSize = 0;

    if (!RtlValidSid(Sid))
    {
        status = STATUS_INVALID_PARAMETER;
        GOTO_CLEANUP();
    }

    status = RtlpGetAceLocationFromOffset(
                    Acl,
                    sizeUsed,
                    AceOffset,
                    OUT_PPVOID(&aceLocation));
    GOTO_CLEANUP_ON_STATUS(status);

    aceSize = RtlLengthAccessDeniedAce(Sid);

    status = RtlpMakeRoomForAceAtLocation(Acl, sizeUsed, aceLocation, aceSize);
    GOTO_CLEANUP_ON_STATUS(status);

    // We know we have at least aceSize bytes available.
    status = RtlInitializeAccessDeniedAce(
                    aceLocation,
                    aceSize,
                    AceFlags,
                    AccessMask,
                    Sid);
    GOTO_CLEANUP_ON_STATUS(status);

    assert(aceSize == aceLocation->Header.AceSize);
    sizeUsed += aceSize;
    Acl->AceCount++;

cleanup:
    if (!NT_SUCCESS(status))
    {
        aceLocation = NULL;
    }
    else
    {
        *AclSizeUsed = sizeUsed;
    }

    *Ace = aceLocation;

    return status;
}

static
NTSTATUS
RtlpRemoveAceFromLocation(
    IN OUT PACL Acl,
    IN OUT PUSHORT AclSizeUsed,
    IN OUT PACE_HEADER AceLocation
    )
{
    NTSTATUS status = STATUS_SUCCESS;
    USHORT sizeUsed = *AclSizeUsed;
    PACE_HEADER aceLocation = AceLocation;
    PVOID aceEnd = NULL;
    PVOID aclEnd = NULL;
    USHORT removedAceSize = 0;

    if (aceLocation->AceSize < sizeof(ACE_HEADER))
    {
        status = STATUS_INVALID_ACL;
        GOTO_CLEANUP();
    }

    aceEnd = LW_PTR_ADD(aceLocation, aceLocation->AceSize);
    aclEnd = LW_PTR_ADD(Acl, sizeUsed);
    if (aceEnd > aclEnd)
    {
        status = STATUS_INVALID_ACL;
        GOTO_CLEANUP();
    }

    removedAceSize = aceLocation->AceSize;

    // Remove the ACE.
    RtlCopyMemory(aceLocation, aceEnd, LW_PTR_OFFSET(aceEnd, aclEnd));
    RtlZeroMemory(LW_PTR_ADD(aclEnd, -removedAceSize), removedAceSize);
    Acl->AceCount--;
    sizeUsed -= removedAceSize;

cleanup:
    if (NT_SUCCESS(status))
    {
        *AclSizeUsed = sizeUsed;
    }

    return status;
}

NTSTATUS
RtlRemoveAce(
    IN OUT PACL Acl,
    IN OUT PUSHORT AclSizeUsed,
    IN USHORT AceOffset
    )
{
    NTSTATUS status = STATUS_SUCCESS;
    USHORT sizeUsed = *AclSizeUsed;
    PACE_HEADER aceLocation = NULL;

    if (AceOffset == ACL_END_ACE_OFFSET)
    {
        status = STATUS_INVALID_PARAMETER;
        GOTO_CLEANUP();
    }

    status = RtlpGetAceLocationFromOffset(
                    Acl,
                    sizeUsed,
                    AceOffset,
                    OUT_PPVOID(&aceLocation));
    GOTO_CLEANUP_ON_STATUS(status);

    status = RtlpRemoveAceFromLocation(Acl, &sizeUsed, aceLocation);
    GOTO_CLEANUP_ON_STATUS(status);

cleanup:
    if (NT_SUCCESS(status))
    {
        *AclSizeUsed = sizeUsed;
    }

    return status;
}

NTSTATUS
RtlIterateAce(
    IN PACL Acl,
    IN USHORT AclSizeUsed,
    IN OUT PUSHORT AceOffset,
    OUT PACE_HEADER* AceHeader
    )
{
    NTSTATUS status = STATUS_SUCCESS;
    USHORT aceOffset = *AceOffset;
    PACE_HEADER aceLocation = NULL;
    PVOID aceEnd = NULL;
    PVOID aclEnd = NULL;

    if (aceOffset == ACL_END_ACE_OFFSET)
    {
        status = STATUS_NO_MORE_ENTRIES;
        GOTO_CLEANUP();
    }

    if ((aceOffset == 0) &&
        (Acl->AceCount == 0))
    {
        status = STATUS_NO_MORE_ENTRIES;
        GOTO_CLEANUP();
    }

    status = RtlpGetAceLocationFromOffset(
                    Acl,
                    AclSizeUsed,
                    aceOffset,
                    OUT_PPVOID(&aceLocation));
    GOTO_CLEANUP_ON_STATUS(status);

    if (aceLocation->AceSize < sizeof(ACE_HEADER))
    {
        status = STATUS_INVALID_ACL;
        GOTO_CLEANUP();
    }

    aceEnd = LW_PTR_ADD(aceLocation, aceLocation->AceSize);
    aclEnd = LW_PTR_ADD(Acl, AclSizeUsed);
    if (aceEnd > aclEnd)
    {
        status = STATUS_INVALID_ACL;
        GOTO_CLEANUP();
    }

    if (aceEnd == aclEnd)
    {
        aceOffset = ACL_END_ACE_OFFSET;
    }
    else
    {
        aceOffset += aceLocation->AceSize;
    }

cleanup:
    if (!NT_SUCCESS(status))
    {
        aceLocation = NULL;
    }
    else
    {
        *AceOffset = aceOffset;
    }

    *AceHeader = aceLocation;

    return status;
}

UCHAR
RtlGetAclRevision(
    IN PACL Acl
    )
{
    return Acl->AclRevision;
}

USHORT
RtlGetAclSize(
    IN PACL Acl
    )
{
    return Acl->AclSize;
}

USHORT
RtlGetAclAceCount(
    IN PACL Acl
    )
{
    return Acl->AceCount;
}

USHORT
RtlGetAclSizeUsed(
    IN PACL Acl
    )
{
    USHORT sizeUsed = sizeof(ACL);
    USHORT aceIndex = 0;

    for (aceIndex = 0; aceIndex < Acl->AceCount; aceIndex++)
    {
        PACE_HEADER ace = (PACE_HEADER) LW_PTR_ADD(Acl, sizeUsed);
        // TODO-Overflow protection
        sizeUsed += ace->AceSize;
    }

    return sizeUsed;
}

BOOLEAN
RtlValidAcl(
    IN PACL Acl,
    OUT OPTIONAL PUSHORT AclSizeUsed
    )
{
    BOOLEAN isValid = TRUE;
    USHORT sizeUsed = sizeof(ACL);
    USHORT aceIndex = 0;

    if (!RtlpValidAclHeader(Acl))
    {
        isValid = FALSE;
        GOTO_CLEANUP();
    }

    for (aceIndex = 0; aceIndex < Acl->AceCount; aceIndex++)
    {
        PACE_HEADER ace = (PACE_HEADER) LW_PTR_ADD(Acl, sizeUsed);
        if (!RtlpValidAce(ace))
        {
            isValid = FALSE;
            GOTO_CLEANUP();
        }
        sizeUsed += ace->AceSize;
        // Check for integer overflow
        if (sizeUsed < ace->AceSize)
        {
            isValid = FALSE;
            GOTO_CLEANUP();
        }
        // Check that not past the end of the ACL
        if (sizeUsed > Acl->AclSize)
        {
            isValid = FALSE;
            GOTO_CLEANUP();
        }
    }

cleanup:
    if (AclSizeUsed)
    {
        *AclSizeUsed = isValid ? sizeUsed : 0;
    }

    return isValid;
}

BOOLEAN
RtlValidAceOffset(
    IN PACL Acl,
    IN USHORT AceOffset
    )
{
    BOOLEAN isValid = FALSE;
    USHORT sizeUsed = sizeof(ACL);
    USHORT aceIndex = 0;

    if (!RtlpValidAclHeader(Acl))
    {
        isValid = FALSE;
        GOTO_CLEANUP();
    }

    for (aceIndex = 0; aceIndex < Acl->AceCount; aceIndex++)
    {
        PACE_HEADER ace = (PACE_HEADER) LW_PTR_ADD(Acl, sizeUsed);
        if (AceOffset == (sizeUsed - sizeof(ACL)))
        {
            isValid = TRUE;
            break;
        }
        sizeUsed += ace->AceSize;
        // Check for integer overflow
        if (sizeUsed < ace->AceSize)
        {
            isValid = FALSE;
            GOTO_CLEANUP();
        }
        // Check that not past the end of the ACL
        if (sizeUsed > Acl->AclSize)
        {
            isValid = FALSE;
            GOTO_CLEANUP();
        }
    }

cleanup:
    return isValid;
}

//
// ACL Functions
//

NTSTATUS
RtlCreateAcl(
    OUT PACL Acl,
    IN ULONG AclLength,
    IN ULONG AclRevision
    )
{
    USHORT aclSizeUsed = 0;
    return RtlInitializeAcl(Acl, &aclSizeUsed, AclLength, AclRevision);
}

static
NTSTATUS
RtlpGetAceLocationFromIndex(
    IN PACL Acl,
    IN ULONG AceIndex,
    OUT PUSHORT AclSizeUsed,
    OUT PUSHORT AceOffset,
    OUT PVOID* AceLocation
    )
{
    NTSTATUS status = STATUS_SUCCESS;
    USHORT sizeUsed = sizeof(ACL);
    PVOID aceLocation = NULL;
    USHORT aceIndex = 0;
    USHORT aceOffset = 0;

    if ((AceIndex >= Acl->AceCount) &&
        (AceIndex != ((ULONG)-1)))
    {
        status = STATUS_INVALID_PARAMETER;
        GOTO_CLEANUP();
    }

    if (!RtlpValidAclHeader(Acl))
    {
        status = STATUS_INVALID_ACL;
        GOTO_CLEANUP();
    }

    for (aceIndex = 0; aceIndex < Acl->AceCount; aceIndex++)
    {
        PACE_HEADER ace = (PACE_HEADER) LW_PTR_ADD(Acl, sizeUsed);
        if (!RtlpValidAce(ace))
        {
            status = STATUS_INVALID_ACL;
            GOTO_CLEANUP();
        }
        if (aceIndex == AceIndex)
        {
            aceOffset = sizeUsed - sizeof(ACL);
        }
        sizeUsed += ace->AceSize;
        // Check for integer overflow
        if (sizeUsed < ace->AceSize)
        {
            status = STATUS_INVALID_ACL;
            GOTO_CLEANUP();
        }
        // Check that not past the end of the ACL
        if (sizeUsed > Acl->AclSize)
        {
            status = STATUS_INVALID_ACL;
            GOTO_CLEANUP();
        }
    }

    // We would not find the offset *iff* we are looking for the end.
    if (0 == aceOffset)
    {
        if (((ULONG)-1) == AceIndex)
        {
            aceOffset = sizeUsed - sizeof(ACL);
        }
        else if (0 != AceIndex)
        {
            status = STATUS_ASSERTION_FAILURE;
            GOTO_CLEANUP();
        }
    }

    if (sizeUsed > Acl->AclSize)
    {
        status = STATUS_INVALID_ACL;
        GOTO_CLEANUP();
    }

    aceLocation = LW_PTR_ADD(Acl, sizeof(ACL) + aceOffset);

cleanup:
    if (!NT_SUCCESS(status))
    {
        sizeUsed = 0;
        aceOffset = 0;
        aceLocation = NULL;
    }

    *AclSizeUsed = sizeUsed;
    *AceOffset = aceOffset;
    *AceLocation = aceLocation;

    return status;
}

static
NTSTATUS
RtlpValidAceList(
    IN ULONG AceRevision,
    IN PVOID AceList,
    IN ULONG AceListLength,
    OUT PUSHORT AceCount
    )
{
    NTSTATUS status = STATUS_SUCCESS;
    ULONG aceOffset = 0;
    USHORT aceCount = 0;

    if ((0 == AceListLength) ||
        (AceListLength > (((USHORT)-1) - sizeof(ACL))))
    {
        status = STATUS_INVALID_PARAMETER;
        GOTO_CLEANUP();
    }

    // Validate ACE list                    
    while (aceOffset < AceListLength)
    {
        PACE_HEADER aceHeader = (PACE_HEADER) LW_PTR_ADD(AceList, aceOffset);

        if ((sizeof(ACE_HEADER) + aceOffset) > AceListLength)
        {
            status = STATUS_INVALID_PARAMETER;
            GOTO_CLEANUP();
        }

        if (((ULONG) aceOffset + aceHeader->AceSize) > AceListLength)
        {
            status = STATUS_INVALID_PARAMETER;
            GOTO_CLEANUP();
        }

        if (!RtlpValidAce(aceHeader))
        {
            status = STATUS_INVALID_PARAMETER;
            GOTO_CLEANUP();
        }

        // TODO-Check ACE type vs ACL revision

        aceCount++;
        aceOffset += aceHeader->AceSize;
    }

    // The list length should match.
    if (aceOffset != AceListLength)
    {
        status = STATUS_INVALID_PARAMETER;
        GOTO_CLEANUP();
    }

cleanup:
    if (!NT_SUCCESS(status))
    {
        aceCount = 0;
    }

    *AceCount = aceCount;

    return status;
}

NTSTATUS
RtlAddAce(
    IN OUT PACL Acl,
    IN ULONG AceRevision,
    IN ULONG StartingAceIndex,
    IN PVOID AceList,
    IN ULONG AceListLength
    )
{
    NTSTATUS status = STATUS_SUCCESS;
    USHORT sizeUsed = 0;
    USHORT aceOffset = 0;
    PVOID aceLocation = NULL;
    USHORT aceListCount = 0;

    if (AceListLength == 0)
    {
        status = STATUS_SUCCESS;
        GOTO_CLEANUP();
    }

    if (!AceList)
    {
        status = STATUS_INVALID_PARAMETER;
        GOTO_CLEANUP();
    }

    if (!RtlpValidAclHeader(Acl))
    {
        status = STATUS_INVALID_ACL;
        GOTO_CLEANUP();
    }

    if (!RtlpValidAclRevision(AceRevision) ||
        (AceRevision > Acl->AclRevision))
    {
        status = STATUS_INVALID_PARAMETER;
        GOTO_CLEANUP();
    }

    // Quick check to bail early if we can.
    if (AceListLength > (Acl->AclSize - sizeof(ACL)))
    {
        status = STATUS_BUFFER_TOO_SMALL;
        GOTO_CLEANUP();
    }

    status = RtlpValidAceList(
                    AceRevision,
                    AceList,
                    AceListLength,
                    &aceListCount);
    GOTO_CLEANUP_ON_STATUS(status);

    status = RtlpGetAceLocationFromIndex(
                    Acl,
                    StartingAceIndex,
                    &sizeUsed,
                    &aceOffset,
                    &aceLocation);
    GOTO_CLEANUP_ON_STATUS(status);

    status = RtlpMakeRoomForAceAtLocation(Acl, sizeUsed, aceLocation, AceListLength);
    GOTO_CLEANUP_ON_STATUS(status);

    // Copy in the ACE list
    RtlCopyMemory(aceLocation, AceList, AceListLength);
    sizeUsed += AceListLength;
    Acl->AceCount += aceListCount;

cleanup:
    return status;
}

NTSTATUS
RtlDeleteAce(
    IN OUT PACL Acl,
    IN ULONG AceIndex
    )
{
    NTSTATUS status = STATUS_SUCCESS;
    USHORT sizeUsed = 0;
    USHORT aceOffset = 0;
    PACE_HEADER aceLocation = NULL;

    if (AceIndex == ((ULONG)-1))
    {
        status = STATUS_INVALID_PARAMETER;
        GOTO_CLEANUP();
    }

    status = RtlpGetAceLocationFromIndex(
                    Acl,
                    AceIndex,
                    &sizeUsed,
                    &aceOffset,
                    OUT_PPVOID(&aceLocation));
    GOTO_CLEANUP_ON_STATUS(status);

    status = RtlpRemoveAceFromLocation(Acl, &sizeUsed, aceLocation);
    GOTO_CLEANUP_ON_STATUS(status);

cleanup:
    return status;
}


NTSTATUS
RtlGetAce(
    IN PACL Acl,
    IN ULONG AceIndex,
    OUT PVOID* Ace
    )
{
    NTSTATUS status = STATUS_SUCCESS;
    USHORT sizeUsed = 0;
    USHORT aceOffset = 0;
    PACE_HEADER aceLocation = NULL;

    if (AceIndex == ((ULONG)-1))
    {
        status = STATUS_INVALID_PARAMETER;
        GOTO_CLEANUP();
    }

    status = RtlpGetAceLocationFromIndex(
                    Acl,
                    AceIndex,
                    &sizeUsed,
                    &aceOffset,
                    OUT_PPVOID(&aceLocation));
    GOTO_CLEANUP_ON_STATUS(status);

cleanup:
    if (!NT_SUCCESS(status))
    {
        aceLocation = NULL;
    }

    *Ace = aceLocation;

    return status;
}

static
SHORT
RtlpCompareAceFlagsForInsert(
    ULONG AceFlags1,
    ULONG AceFlags2
    )
{
    SHORT sResult = 0;
    BOOLEAN bInherited1 = (AceFlags1 & INHERITED_ACE) ? TRUE : FALSE;
    BOOLEAN bInheritOnly1 = (AceFlags1 & INHERIT_ONLY_ACE) ? TRUE : FALSE;
    BOOLEAN bInherited2 = (AceFlags2 & INHERITED_ACE) ? TRUE : FALSE;
    BOOLEAN bInheritOnly2 = (AceFlags2 & INHERIT_ONLY_ACE) ? TRUE : FALSE;

    // Sort order is:
    // 1. Non-inherited, inherit-only
    // 2. Non-inherited
    // 3. Inherited, inherit only
    // 4. inherited

    if (bInherited1)
    {
        if (!bInherited2)
        {
            sResult = 1;
            GOTO_CLEANUP();
        }

        // Comparing two inherited ACEs

        if (bInheritOnly1)
        {
            if (!bInheritOnly2)
            {
                sResult = -1;
                GOTO_CLEANUP();
            }

            sResult = 0;
            GOTO_CLEANUP();
        }

        // Ace1: Inherited, non-inherit only
        // Ace2: Inherited, ...

        if (bInheritOnly2)
        {
            sResult = 1;
            GOTO_CLEANUP();
        }

        sResult = 0;
        GOTO_CLEANUP();
    }
    else  // Non-inherited
    {
        if (bInherited2)
        {
            sResult = -1;
            GOTO_CLEANUP();
        }

        // Comparing two non-inherited ACEs

        if (bInheritOnly1)
        {
            if (!bInheritOnly2)
            {
                sResult = -1;
                GOTO_CLEANUP();
            }

            sResult = 0;
            GOTO_CLEANUP();
        }

        // Ace1: Non-Inherited, non-inherit only
        // Ace2: Non-Inherited, ...

        if (bInheritOnly2)
        {
            sResult = 1;
            GOTO_CLEANUP();
        }

        sResult = 0;
        GOTO_CLEANUP();
    }

cleanup:
    return sResult;
}


NTSTATUS
RtlAddAccessAllowedAceEx(
    IN PACL Acl,
    IN ULONG AceRevision,
    IN ULONG AceFlags,
    IN ACCESS_MASK AccessMask,
    IN PSID Sid
    )
{
    NTSTATUS status = STATUS_SUCCESS;
    USHORT sizeUsed = 0;
    PACCESS_ALLOWED_ACE aceLocation = NULL;
    USHORT aceSize = 0;
    USHORT usLoop = 0;
    USHORT aceOffset = 0;
    PACE_HEADER aceHeader = NULL;

    if (!RtlpValidAclHeader(Acl))
    {
        status = STATUS_INVALID_ACL;
        GOTO_CLEANUP();
    }

    if (!RtlpValidAclRevision(AceRevision) ||
        (AceRevision > Acl->AclRevision))
    {
        status = STATUS_INVALID_PARAMETER;
        GOTO_CLEANUP();
    }

    if (!RtlValidSid(Sid))
    {
        status = STATUS_INVALID_PARAMETER;
        GOTO_CLEANUP();
    }

    // Allowed ACEs get added to the front of the allow list

    if (Acl->AceCount > 0)
    {
        sizeUsed = RtlGetAclSizeUsed(Acl);

        for (usLoop=0; usLoop < Acl->AceCount; usLoop++)
        {
            status = RtlIterateAce(Acl, sizeUsed, &aceOffset, &aceHeader);
            GOTO_CLEANUP_ON_STATUS(status);

            if (aceHeader->AceType == ACCESS_ALLOWED_ACE_TYPE)
            {
                if (RtlpCompareAceFlagsForInsert(
                        AceFlags, 
                        aceHeader->AceFlags) <= 0)
                {
                    break;
                }
            }
        }

        aceLocation = (PACCESS_ALLOWED_ACE)aceHeader;
    }

    // Covers Acl->AceCount == 0 and the case where we are adding 
    // to the end of the list

    if (usLoop == Acl->AceCount)
    {
        status = RtlpGetAceLocationFromIndex(
                     Acl,
                     ((ULONG)-1),
                     &sizeUsed,
                     &aceOffset,
                     OUT_PPVOID(&aceLocation));
        GOTO_CLEANUP_ON_STATUS(status);
    }

    aceSize = RtlLengthAccessAllowedAce(Sid);

    status = RtlpMakeRoomForAceAtLocation(Acl, sizeUsed, aceLocation, aceSize);
    GOTO_CLEANUP_ON_STATUS(status);

    // We know we have at least aceSize bytes available.
    status = RtlInitializeAccessAllowedAce(
                    aceLocation,
                    aceSize,
                    AceFlags,
                    AccessMask,
                    Sid);
    GOTO_CLEANUP_ON_STATUS(status);

    assert(aceSize == aceLocation->Header.AceSize);
    sizeUsed += aceSize;
    Acl->AceCount++;

cleanup:
    return status;
}

NTSTATUS
RtlAddAccessDeniedAceEx(
    IN PACL Acl,
    IN ULONG AceRevision,
    IN ULONG AceFlags,
    IN ACCESS_MASK AccessMask,
    IN PSID Sid
    )
{
    NTSTATUS status = STATUS_SUCCESS;
    USHORT sizeUsed = 0;
    PACCESS_DENIED_ACE aceLocation = NULL;
    USHORT aceSize = 0;
    USHORT usLoop = 0;
    USHORT aceOffset = 0;
    PACE_HEADER aceHeader = NULL;

    if (!RtlpValidAclHeader(Acl))
    {
        status = STATUS_INVALID_ACL;
        GOTO_CLEANUP();
    }

    if (!RtlpValidAclRevision(AceRevision) ||
        (AceRevision > Acl->AclRevision))
    {
        status = STATUS_INVALID_PARAMETER;
        GOTO_CLEANUP();
    }

    if (!RtlValidSid(Sid))
    {
        status = STATUS_INVALID_PARAMETER;
        GOTO_CLEANUP();
    }

    // Deny ACEs get added to the end of the deny list

    if (Acl->AceCount > 0)
    {
        sizeUsed = RtlGetAclSizeUsed(Acl);

        for (usLoop=0; usLoop < Acl->AceCount; usLoop++)
        {
            status = RtlIterateAce(Acl, sizeUsed, &aceOffset, &aceHeader);
            GOTO_CLEANUP_ON_STATUS(status);

            if (aceHeader->AceType == ACCESS_ALLOWED_ACE_TYPE)
            {
                // Acess Denied ACE always goes before Access Allowed
                break;
            }

            if (aceHeader->AceType == ACCESS_DENIED_ACE_TYPE)
            {
                if (RtlpCompareAceFlagsForInsert(
                        AceFlags, 
                        aceHeader->AceFlags) <= 0)
                {
                    break;
                }
            }
        }

        aceLocation = (PACCESS_DENIED_ACE)aceHeader;
    }

    // Covers Acl->AceCount == 0 and the case where we are adding 
    // to the end of the list

    if (usLoop == Acl->AceCount)
    {
        status = RtlpGetAceLocationFromIndex(
                     Acl,
                     ((ULONG)-1),
                     &sizeUsed,
                     &aceOffset,
                     OUT_PPVOID(&aceLocation));
        GOTO_CLEANUP_ON_STATUS(status);
    }

    aceSize = RtlLengthAccessDeniedAce(Sid);

    status = RtlpMakeRoomForAceAtLocation(Acl, sizeUsed, aceLocation, aceSize);
    GOTO_CLEANUP_ON_STATUS(status);

    // We know we have at least aceSize bytes available.
    status = RtlInitializeAccessDeniedAce(
                    aceLocation,
                    aceSize,
                    AceFlags,
                    AccessMask,
                    Sid);
    GOTO_CLEANUP_ON_STATUS(status);

    assert(aceSize == aceLocation->Header.AceSize);
    sizeUsed += aceSize;
    Acl->AceCount++;

cleanup:
    return status;
}

NTSTATUS
RtlAddSystemAuditAceEx(
    IN PACL Acl,
    IN ULONG AceRevision,
    IN ULONG AceFlags,
    IN ACCESS_MASK AccessMask,
    IN PSID Sid
    )
{
    NTSTATUS status = STATUS_SUCCESS;
    USHORT sizeUsed = 0;
    PSYSTEM_AUDIT_ACE aceLocation = NULL;
    USHORT aceSize = 0;
    USHORT aceOffset = 0;

    if (!RtlpValidAclHeader(Acl))
    {
        status = STATUS_INVALID_ACL;
        GOTO_CLEANUP();
    }

    if (!RtlpValidAclRevision(AceRevision) ||
        (AceRevision > Acl->AclRevision))
    {
        status = STATUS_INVALID_PARAMETER;
        GOTO_CLEANUP();
    }

    if (!RtlValidSid(Sid))
    {
        status = STATUS_INVALID_PARAMETER;
        GOTO_CLEANUP();
    }

    // Add to end of list

    status = RtlpGetAceLocationFromIndex(
                 Acl,
                 ((ULONG)-1),
                 &sizeUsed,
                 &aceOffset,
                 OUT_PPVOID(&aceLocation));
    GOTO_CLEANUP_ON_STATUS(status);

    aceSize = RtlLengthAccessDeniedAce(Sid);

    status = RtlpMakeRoomForAceAtLocation(Acl, sizeUsed, aceLocation, aceSize);
    GOTO_CLEANUP_ON_STATUS(status);

    // We know we have at least aceSize bytes available.
    status = RtlInitializeSystemAuditAce(
                    aceLocation,
                    aceSize,
                    AceFlags,
                    AccessMask,
                    Sid);
    GOTO_CLEANUP_ON_STATUS(status);

    assert(aceSize == aceLocation->Header.AceSize);
    sizeUsed += aceSize;
    Acl->AceCount++;

cleanup:
    return status;
}

BOOLEAN
RtlpIsValidLittleEndianAclBuffer(
    IN PVOID Buffer,
    IN ULONG BufferSize,
    OUT PULONG BufferUsed
    )
{
    NTSTATUS status = STATUS_SUCCESS;
    PACL littleEndianAcl = (PACL) Buffer;
    ACL aclHeader = { 0 };
    ULONG i = 0;
    ULONG offset = 0;

    if (BufferSize < ACL_HEADER_SIZE)
    {
        status = STATUS_INVALID_ACL;
        GOTO_CLEANUP();
    }

    aclHeader.AclRevision = LW_LTOH8(littleEndianAcl->AclRevision);
    aclHeader.Sbz1 = LW_LTOH8(littleEndianAcl->Sbz1);
    aclHeader.AclSize = LW_LTOH16(littleEndianAcl->AclSize);
    aclHeader.AceCount = LW_LTOH16(littleEndianAcl->AceCount);
    aclHeader.Sbz2 = LW_LTOH16(littleEndianAcl->Sbz2);

    if (!RtlpValidAclHeader(&aclHeader))
    {
        status = STATUS_INVALID_ACL;
        GOTO_CLEANUP();
    }

    if (!RtlpIsBufferAvailable(BufferSize, 0, aclHeader.AclSize))
    {
        status = STATUS_INVALID_ACL;
        GOTO_CLEANUP();
    }

    offset = ACL_HEADER_SIZE;
    for (i = 0; i < aclHeader.AceCount; i++)
    {
        PACE_HEADER littlEndianAceHeader = (PACE_HEADER) LwRtlOffsetToPointer(littleEndianAcl, offset);
        ACE_HEADER aceHeader = { 0 };

        aceHeader.AceType = LW_LTOH8(littlEndianAceHeader->AceType);
        aceHeader.AceFlags = LW_LTOH8(littlEndianAceHeader->AceFlags);
        aceHeader.AceSize = LW_LTOH16(littlEndianAceHeader->AceSize);

        if (!RtlpValidAceHeader(&aceHeader))
        {
            status = STATUS_INVALID_ACL;
            GOTO_CLEANUP();
        }

        if (!RtlpIsBufferAvailable(aclHeader.AclSize, offset, aceHeader.AceSize))
        {
            status = STATUS_INVALID_ACL;
            GOTO_CLEANUP();
        }

        switch (aceHeader.AceType)
        {
            case ACCESS_ALLOWED_ACE_TYPE:
            case ACCESS_DENIED_ACE_TYPE:
            case SYSTEM_AUDIT_ACE_TYPE:
            {
                PACCESS_ALLOWED_ACE littleEndianAce = (PACCESS_ALLOWED_ACE) littlEndianAceHeader;
                ACCESS_ALLOWED_ACE ace = { { 0 } };

                ace.Header = aceHeader;
                ace.Mask = LW_LTOH32(littleEndianAce->Mask);
                // Just need the first ULONG, which is already correct byte
                // order because it really is a sequence of UCHARs.
                ace.SidStart = littleEndianAce->SidStart;

                status = RtlpVerifyAceEx(&ace.Header, TRUE);
                GOTO_CLEANUP_ON_STATUS(status);

                break;
            }
            case ACCESS_ALLOWED_OBJECT_ACE_TYPE:
            case ACCESS_DENIED_OBJECT_ACE_TYPE:
            case SYSTEM_AUDIT_OBJECT_ACE_TYPE:
            {
                PACCESS_ALLOWED_OBJECT_ACE littleEndianAce = (PACCESS_ALLOWED_OBJECT_ACE) littlEndianAceHeader;
                ACCESS_ALLOWED_OBJECT_ACE ace = { { 0 } };

                ace.Header = aceHeader;
                ace.Mask = LW_LTOH32(littleEndianAce->Mask);
                ace.Flags = LW_LTOH32(littleEndianAce->Flags);

                ace.SidStart = littleEndianAce->SidStart;
                ace.ObjectType = littleEndianAce->ObjectType;
                ace.InheritedObjectType = littleEndianAce->InheritedObjectType;

                status = RtlpVerifyAceEx(&ace.Header, TRUE);
                GOTO_CLEANUP_ON_STATUS(status);

                break;
            }
            default:
                status = STATUS_INVALID_ACL;
                GOTO_CLEANUP();
        }
        // No overflow possible because we already checked buffer
        // availability.
        offset += aceHeader.AceSize;
    }

cleanup:
    *BufferUsed = NT_SUCCESS(status) ? aclHeader.AclSize : 0;

    return NT_SUCCESS(status);
}

NTSTATUS
RtlpEncodeLittleEndianAcl(
    IN PACL Acl,
    OUT PVOID Buffer,
    IN ULONG BufferSize,
    OUT PULONG BufferUsed
    )
{
    NTSTATUS status = STATUS_SUCCESS;
    PACL littleEndianAcl = (PACL) Buffer;
    USHORT size = 0;
    ULONG i = 0;
    ULONG offset = 0;

    if (!RtlValidAcl(Acl, &size))
    {
        status = STATUS_INVALID_ACL;
        GOTO_CLEANUP();
    }

    if (BufferSize < size)
    {
        status = STATUS_BUFFER_TOO_SMALL;
        GOTO_CLEANUP();
    }

    littleEndianAcl->AclRevision = LW_HTOL8(Acl->AclRevision);
    littleEndianAcl->Sbz1 = LW_HTOL8(Acl->Sbz1);
    littleEndianAcl->AclSize = LW_HTOL16(size);
    littleEndianAcl->AceCount = LW_HTOL16(Acl->AceCount);
    littleEndianAcl->Sbz2 = LW_HTOL16(Acl->Sbz2);

    // Handle ACEs.  Note that the size has already been verified.
    offset = sizeof(ACL);
    for (i = 0; i < Acl->AceCount; i++)
    {
        PACE_HEADER aceHeader = (PACE_HEADER) LwRtlOffsetToPointer(Acl, offset);
        PACE_HEADER littleEndianAceHeader = (PACE_HEADER) LwRtlOffsetToPointer(littleEndianAcl, offset);
        ULONG sidSizeUsed = 0;

        littleEndianAceHeader->AceType = LW_HTOL8(aceHeader->AceType);
        littleEndianAceHeader->AceFlags = LW_HTOL8(aceHeader->AceFlags);
        littleEndianAceHeader->AceSize = LW_HTOL16(aceHeader->AceSize);

        switch (aceHeader->AceType)
        {
            case ACCESS_ALLOWED_ACE_TYPE:
            case ACCESS_DENIED_ACE_TYPE:
            case SYSTEM_AUDIT_ACE_TYPE:
            {
                PACCESS_ALLOWED_ACE ace = (PACCESS_ALLOWED_ACE) aceHeader;
                PACCESS_ALLOWED_ACE littleEndianAce = (PACCESS_ALLOWED_ACE) littleEndianAceHeader;

                littleEndianAce->Mask = LW_HTOL32(ace->Mask);

                status = RtlpEncodeLittleEndianSid(
                                (PSID) &ace->SidStart,
                                &littleEndianAce->SidStart,
                                aceHeader->AceSize - sizeof(ACE_HEADER),
                                &sidSizeUsed);
                if (!NT_SUCCESS(status))
                {
                    status = STATUS_ASSERTION_FAILURE;
                }
                GOTO_CLEANUP_ON_STATUS(status);
                break;
            }
            default:
                status = STATUS_INVALID_ACL;
                GOTO_CLEANUP();
        }

        offset += aceHeader->AceSize;
    }

    if (offset != size)
    {
        status = STATUS_ASSERTION_FAILURE;
        GOTO_CLEANUP();
    }

    status = STATUS_SUCCESS;

cleanup:
    *BufferUsed = NT_SUCCESS(status) ? size : 0;

    return status;
}

NTSTATUS
RtlpDecodeLittleEndianAcl(
    IN PACL LittleEndianAcl,
    OUT PACL Acl
    )
{
    NTSTATUS status = STATUS_SUCCESS;
    ULONG offset = 0;
    ULONG i = 0;

    Acl->AclRevision = LW_LTOH8(LittleEndianAcl->AclRevision);
    Acl->Sbz1 = LW_LTOH8(LittleEndianAcl->Sbz1);
    Acl->AclSize = LW_LTOH16(LittleEndianAcl->AclSize);
    Acl->AceCount = LW_LTOH16(LittleEndianAcl->AceCount);
    Acl->Sbz2 = LW_LTOH16(LittleEndianAcl->Sbz2);

    // Handle ACEs.
    offset = sizeof(ACL);
    for (i = 0; i < Acl->AceCount; i++)
    {
        PACE_HEADER littleEndianAceHeader = (PACE_HEADER) LwRtlOffsetToPointer(LittleEndianAcl, offset);
        PACE_HEADER aceHeader = (PACE_HEADER) LwRtlOffsetToPointer(Acl, offset);

        aceHeader->AceType = LW_LTOH8(littleEndianAceHeader->AceType);
        aceHeader->AceFlags = LW_LTOH8(littleEndianAceHeader->AceFlags);
        aceHeader->AceSize = LW_LTOH16(littleEndianAceHeader->AceSize);

        switch (aceHeader->AceType)
        {
            case ACCESS_ALLOWED_ACE_TYPE:
            case ACCESS_DENIED_ACE_TYPE:
            case SYSTEM_AUDIT_ACE_TYPE:
            {
                PACCESS_ALLOWED_ACE littleEndianAce = (PACCESS_ALLOWED_ACE) littleEndianAceHeader;
                PACCESS_ALLOWED_ACE ace = (PACCESS_ALLOWED_ACE) aceHeader;

                ace->Mask = LW_LTOH32(littleEndianAce->Mask);

                RtlpDecodeLittleEndianSid(
                        (PSID) &littleEndianAce->SidStart,
                        (PSID) &ace->SidStart);
                break;
            }
            case ACCESS_ALLOWED_OBJECT_ACE_TYPE:
            case ACCESS_DENIED_OBJECT_ACE_TYPE:
            case SYSTEM_AUDIT_OBJECT_ACE_TYPE:
            {
                PACCESS_ALLOWED_OBJECT_ACE littleEndianAce = (PACCESS_ALLOWED_OBJECT_ACE) littleEndianAceHeader;
                PACCESS_ALLOWED_OBJECT_ACE ace = (PACCESS_ALLOWED_OBJECT_ACE) aceHeader;

                ace->Mask = LW_LTOH32(littleEndianAce->Mask);
                ace->Flags = LW_LTOH32(littleEndianAce->Flags);
                
                /*
                 * GUID arrives with LE Data1 Data2 Data3 members. Libuuid assumes these are in Big Endian so we need to swap
                 * byte ordering.
                 */

                switch (ace->Flags) {
                    case 0:
                        RtlpDecodeLittleEndianSid(
                                (PSID) &littleEndianAce->ObjectType,
                                (PSID) &ace->ObjectType);
                        break;
                    case ACE_OBJECT_TYPE_PRESENT:
                    case ACE_INHERITED_OBJECT_TYPE_PRESENT:
                        ace->ObjectType.Data1 = _LW_SWAB32(littleEndianAce->ObjectType.Data1);
                        ace->ObjectType.Data2 = _LW_SWAB16(littleEndianAce->ObjectType.Data2);
                        ace->ObjectType.Data3 = _LW_SWAB16(littleEndianAce->ObjectType.Data3);
                        memcpy(ace->ObjectType.Data4, littleEndianAce->ObjectType.Data4, sizeof(ace->ObjectType.Data4));

                        RtlpDecodeLittleEndianSid(
                                (PSID) &littleEndianAce->InheritedObjectType,
                                (PSID) &ace->InheritedObjectType);
                        break;
                    default:
                        ace->ObjectType.Data1 = _LW_SWAB32(littleEndianAce->ObjectType.Data1);
                        ace->ObjectType.Data2 = _LW_SWAB16(littleEndianAce->ObjectType.Data2);
                        ace->ObjectType.Data3 = _LW_SWAB16(littleEndianAce->ObjectType.Data3);
                        memcpy(ace->ObjectType.Data4, littleEndianAce->ObjectType.Data4, sizeof(ace->ObjectType.Data4));

                        ace->InheritedObjectType.Data1 = _LW_SWAB32(littleEndianAce->InheritedObjectType.Data1);
                        ace->InheritedObjectType.Data2 = _LW_SWAB16(littleEndianAce->InheritedObjectType.Data2);
                        ace->InheritedObjectType.Data3 = _LW_SWAB16(littleEndianAce->InheritedObjectType.Data3);
                        memcpy(ace->InheritedObjectType.Data4, littleEndianAce->InheritedObjectType.Data4, sizeof(ace->InheritedObjectType.Data4));

                        RtlpDecodeLittleEndianSid(
                                (PSID) &littleEndianAce->SidStart,
                                (PSID) &ace->SidStart);
                        break;
                }

                break;
            }
            default:
                // TODO-Not much we can do here as the caller
                // screwed up big time.  We could return an error
                // code, I suppose, but if this is busted this
                // library is busted wrt self-relative SD verification.
                status = STATUS_INVALID_ACL;
                GOTO_CLEANUP();
        }

        offset += aceHeader->AceSize;
    }

cleanup:
    return status;
}

/*
local variables:
mode: c
c-basic-offset: 4
indent-tabs-mode: nil
tab-width: 4
end:
*/


