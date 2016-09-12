/*
  Dokan : user-mode file system library for Windows

  Copyright (C) 2016 - 2016 Adrien J. <liryna.stark@gmail.com> and Maxime C. <maxime@islog.com>
  Copyright (C) 2007 - 2011 Hiroki Asakawa <info@dokan-dev.net>

  http://dokan-dev.github.io

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU Lesser General Public License as published by the Free
Software Foundation; either version 3 of the License, or (at your option) any
later version.

This program is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU Lesser General Public License along
with this program. If not, see <http://www.gnu.org/licenses/>.
*/

#include "dokan.h"

NTSTATUS
DokanDispatchQueryInformation(__in PDEVICE_OBJECT DeviceObject, __in PIRP Irp) {
  NTSTATUS status = STATUS_NOT_IMPLEMENTED;
  PIO_STACK_LOCATION irpSp;
  PFILE_OBJECT fileObject;
  PDokanCCB ccb;
  PDokanFCB fcb = NULL;
  PDokanVCB vcb;
  ULONG info = 0;
  ULONG eventLength;
  PEVENT_CONTEXT eventContext;

  // PAGED_CODE();

  __try {
    DDbgPrint("==> DokanQueryInformation\n");

    irpSp = IoGetCurrentIrpStackLocation(Irp);
    fileObject = irpSp->FileObject;

    DDbgPrint("  FileInfoClass %d\n",
              irpSp->Parameters.QueryFile.FileInformationClass);
    DDbgPrint("  ProcessId %lu\n", IoGetRequestorProcessId(Irp));

    if (fileObject == NULL) {
      DDbgPrint("  fileObject == NULL\n");
      status = STATUS_INVALID_PARAMETER;
      __leave;
    }

    DokanPrintFileName(fileObject);

    /*
    if (fileObject->FsContext2 == NULL &&
            fileObject->FileName.Length == 0) {
            // volume open?
            status = STATUS_SUCCESS;
            __leave;
    }*/
    vcb = DeviceObject->DeviceExtension;
    if (GetIdentifierType(vcb) != VCB ||
        !DokanCheckCCB(vcb->Dcb, fileObject->FsContext2)) {
      status = STATUS_INVALID_PARAMETER;
      __leave;
    }

    ccb = (PDokanCCB)fileObject->FsContext2;
    ASSERT(ccb != NULL);

    fcb = ccb->Fcb;
    ASSERT(fcb != NULL);
    DokanFCBLockRO(fcb);

    switch (irpSp->Parameters.QueryFile.FileInformationClass) {
    case FileBasicInformation:
      DDbgPrint("  FileBasicInformation\n");
      break;
    case FileInternalInformation:
      DDbgPrint("  FileInternalInformation\n");
      break;
    case FileEaInformation:
      DDbgPrint("  FileEaInformation\n");
      break;
    case FileStandardInformation:
      DDbgPrint("  FileStandardInformation\n");
      break;
    case FileAllInformation:
      DDbgPrint("  FileAllInformation\n");
      break;
    case FileAlternateNameInformation:
      DDbgPrint("  FileAlternateNameInformation\n");
      break;
    case FileAttributeTagInformation:
      DDbgPrint("  FileAttributeTagInformation\n");
      break;
    case FileCompressionInformation:
      DDbgPrint("  FileCompressionInformation\n");
      break;
    case FileNormalizedNameInformation: // Fake implementation by returning
                                        // FileNameInformation result.
                                        // TODO: implement it
      DDbgPrint("  FileNormalizedNameInformation\n");
    case FileNameInformation: {
      PFILE_NAME_INFORMATION nameInfo;

      DDbgPrint("  FileNameInformation\n");

      if (irpSp->Parameters.QueryFile.Length <
          sizeof(FILE_NAME_INFORMATION) + fcb->FileName.Length) {

        info = irpSp->Parameters.QueryFile.Length;
        status = STATUS_BUFFER_OVERFLOW;

      } else {

        nameInfo = (PFILE_NAME_INFORMATION)Irp->AssociatedIrp.SystemBuffer;
        ASSERT(nameInfo != NULL);

        nameInfo->FileNameLength = fcb->FileName.Length;
        RtlCopyMemory(nameInfo->FileName, fcb->FileName.Buffer,
                      fcb->FileName.Length);
        info = FIELD_OFFSET(FILE_NAME_INFORMATION, FileName[0]) +
               fcb->FileName.Length;
        status = STATUS_SUCCESS;
      }
      __leave;
    } break;
    case FileNetworkOpenInformation:
      DDbgPrint("  FileNetworkOpenInformation\n");
      break;
    case FilePositionInformation: {
      PFILE_POSITION_INFORMATION posInfo;

      DDbgPrint("  FilePositionInformation\n");

      if (irpSp->Parameters.QueryFile.Length <
          sizeof(FILE_POSITION_INFORMATION)) {
        status = STATUS_INFO_LENGTH_MISMATCH;

      } else {
        posInfo = (PFILE_POSITION_INFORMATION)Irp->AssociatedIrp.SystemBuffer;
        ASSERT(posInfo != NULL);

        RtlZeroMemory(posInfo, sizeof(FILE_POSITION_INFORMATION));

        if (fileObject->CurrentByteOffset.QuadPart < 0) {
          status = STATUS_INVALID_PARAMETER;
        } else {
          // set the current file offset
          posInfo->CurrentByteOffset = fileObject->CurrentByteOffset;

          info = sizeof(FILE_POSITION_INFORMATION);
          status = STATUS_SUCCESS;
        }
      }
      __leave;
    } break;
    case FileStreamInformation:
      DDbgPrint("  FileStreamInformation\n");
      break;
    case FileStandardLinkInformation:
      DDbgPrint("  FileStandardLinkInformation\n");
      break;
    case FileNetworkPhysicalNameInformation:
      DDbgPrint("  FileNetworkPhysicalNameInformation\n");
      break;
    case FileRemoteProtocolInformation:
      DDbgPrint("  FileRemoteProtocolInformation\n");
      break;
    default:
      DDbgPrint("  unknown type:%d\n",
                irpSp->Parameters.QueryFile.FileInformationClass);
      break;
    }

    // if it is not treadted in swich case

    // calculate the length of EVENT_CONTEXT
    // sum of it's size and file name length
    eventLength = sizeof(EVENT_CONTEXT) + fcb->FileName.Length;

    eventContext = AllocateEventContext(vcb->Dcb, Irp, eventLength, ccb);

    if (eventContext == NULL) {
      status = STATUS_INSUFFICIENT_RESOURCES;
      __leave;
    }

    eventContext->Context = ccb->UserContext;
    // DDbgPrint("   get Context %X\n", (ULONG)ccb->UserContext);

    eventContext->Operation.File.FileInformationClass =
        irpSp->Parameters.QueryFile.FileInformationClass;

    // bytes length which is able to be returned
    eventContext->Operation.File.BufferLength =
        irpSp->Parameters.QueryFile.Length;

    // copy file name to EventContext from FCB
    eventContext->Operation.File.FileNameLength = fcb->FileName.Length;
    RtlCopyMemory(eventContext->Operation.File.FileName, fcb->FileName.Buffer,
                  fcb->FileName.Length);

    // register this IRP to pending IPR list
    status = DokanRegisterPendingIrp(DeviceObject, Irp, eventContext, 0);

  } __finally {
    if(fcb)
      DokanFCBUnlock(fcb);

    DokanCompleteIrpRequest(Irp, status, info);

    DDbgPrint("<== DokanQueryInformation\n");
  }

  return status;
}

VOID DokanCompleteQueryInformation(__in PIRP_ENTRY IrpEntry,
                                   __in PEVENT_INFORMATION EventInfo) {
  PIRP irp;
  PIO_STACK_LOCATION irpSp;
  NTSTATUS status = STATUS_SUCCESS;
  ULONG info = 0;
  ULONG bufferLen = 0;
  PVOID buffer = NULL;
  PDokanCCB ccb;

  DDbgPrint("==> DokanCompleteQueryInformation\n");

  irp = IrpEntry->Irp;
  irpSp = IrpEntry->IrpSp;

  ccb = IrpEntry->FileObject->FsContext2;

  ASSERT(ccb != NULL);

  ccb->UserContext = EventInfo->Context;
  // DDbgPrint("   set Context %X\n", (ULONG)ccb->UserContext);

  // where we shold copy FileInfo to
  buffer = irp->AssociatedIrp.SystemBuffer;

  // available buffer size
  bufferLen = irpSp->Parameters.QueryFile.Length;

  // buffer is not specifed or short of size
  if (bufferLen == 0 || buffer == NULL || bufferLen < EventInfo->BufferLength) {
    info = 0;
    status = STATUS_INSUFFICIENT_RESOURCES;

  } else {

    //
    // we write FileInfo from user mode
    //
    ASSERT(buffer != NULL);

    RtlZeroMemory(buffer, bufferLen);
    RtlCopyMemory(buffer, EventInfo->Buffer, EventInfo->BufferLength);

    // written bytes
    info = EventInfo->BufferLength;
    status = EventInfo->Status;

    if (NT_SUCCESS(status) &&
        irpSp->Parameters.QueryFile.FileInformationClass ==
            FileAllInformation) {

      PFILE_ALL_INFORMATION allInfo = (PFILE_ALL_INFORMATION)buffer;
      allInfo->PositionInformation.CurrentByteOffset =
          IrpEntry->FileObject->CurrentByteOffset;
    }
  }

  DokanCompleteIrpRequest(irp, status, info);

  DDbgPrint("<== DokanCompleteQueryInformation\n");
}

NTSTATUS
DokanDispatchSetInformation(__in PDEVICE_OBJECT DeviceObject, __in PIRP Irp) {

  NTSTATUS status = STATUS_NOT_IMPLEMENTED;
  PIO_STACK_LOCATION irpSp;
  PVOID buffer;
  PFILE_OBJECT fileObject;
  PDokanCCB ccb;
  PDokanFCB fcb = NULL;
  PDokanVCB vcb;
  ULONG eventLength;
  PFILE_OBJECT targetFileObject;
  PEVENT_CONTEXT eventContext;
  BOOLEAN isPagingIo = FALSE;
  PFILE_END_OF_FILE_INFORMATION pInfoEoF = NULL;

  vcb = DeviceObject->DeviceExtension;

  __try {
    DDbgPrint("==> DokanSetInformationn\n");

    irpSp = IoGetCurrentIrpStackLocation(Irp);
    fileObject = irpSp->FileObject;

    if (fileObject == NULL) {
      DDbgPrint("  fileObject == NULL\n");
      status = STATUS_INVALID_PARAMETER;
      __leave;
    }

    if (GetIdentifierType(vcb) != VCB ||
        !DokanCheckCCB(vcb->Dcb, fileObject->FsContext2)) {
      status = STATUS_INVALID_PARAMETER;
      __leave;
    }

    ccb = (PDokanCCB)fileObject->FsContext2;
    ASSERT(ccb != NULL);

    fcb = ccb->Fcb;
    ASSERT(fcb != NULL);
    DokanFCBLockRW(fcb);

    DDbgPrint("  ProcessId %lu\n", IoGetRequestorProcessId(Irp));
    DokanPrintFileName(fileObject);

    buffer = Irp->AssociatedIrp.SystemBuffer;

    if (Irp->Flags & IRP_PAGING_IO) {
      isPagingIo = TRUE;
    }

    switch (irpSp->Parameters.SetFile.FileInformationClass) {
    case FileAllocationInformation:
      DDbgPrint(
          "  FileAllocationInformation %lld\n",
          ((PFILE_ALLOCATION_INFORMATION)buffer)->AllocationSize.QuadPart);
      break;
    case FileBasicInformation:
      DDbgPrint("  FileBasicInformation\n");
      break;
    case FileDispositionInformation:
      DDbgPrint("  FileDispositionInformation\n");
      break;
    case FileEndOfFileInformation:
      if ((fileObject->SectionObjectPointer != NULL) &&
          (fileObject->SectionObjectPointer->DataSectionObject != NULL)) {

        pInfoEoF = (PFILE_END_OF_FILE_INFORMATION)buffer;

        if (!MmCanFileBeTruncated(fileObject->SectionObjectPointer,
                                  &pInfoEoF->EndOfFile)) {
          status = STATUS_USER_MAPPED_FILE;
          __leave;
        }

        if (!isPagingIo) {
          ExAcquireResourceExclusiveLite(&fcb->PagingIoResource, TRUE);
          CcFlushCache(&fcb->SectionObjectPointers, NULL, 0, NULL);
          CcPurgeCacheSection(&fcb->SectionObjectPointers, NULL, 0, FALSE);
          ExReleaseResourceLite(&fcb->PagingIoResource);
        }
      }
      DDbgPrint("  FileEndOfFileInformation %lld\n",
                ((PFILE_END_OF_FILE_INFORMATION)buffer)->EndOfFile.QuadPart);
      break;
    case FileLinkInformation:
      DDbgPrint("  FileLinkInformation\n");
      break;
    case FilePositionInformation: {
      PFILE_POSITION_INFORMATION posInfo;

      posInfo = (PFILE_POSITION_INFORMATION)Irp->AssociatedIrp.SystemBuffer;
      ASSERT(posInfo != NULL);

      DDbgPrint("  FilePositionInformation %lld\n",
                posInfo->CurrentByteOffset.QuadPart);
      fileObject->CurrentByteOffset = posInfo->CurrentByteOffset;

      status = STATUS_SUCCESS;

      __leave;
    } break;
    case FileRenameInformation:
      DDbgPrint("  FileRenameInformation\n");
      break;
    case FileValidDataLengthInformation:
      DDbgPrint("  FileValidDataLengthInformation\n");
      break;
    default:
      DDbgPrint("  unknown type:%d\n",
                irpSp->Parameters.SetFile.FileInformationClass);
      break;
    }

    //
    // when this IRP is not handled in swich case
    //

    // calcurate the size of EVENT_CONTEXT
    // it is sum of file name length and size of FileInformation
    eventLength = sizeof(EVENT_CONTEXT) + fcb->FileName.Length +
                  irpSp->Parameters.SetFile.Length;

    targetFileObject = irpSp->Parameters.SetFile.FileObject;

    if (targetFileObject) {
      DDbgPrint("  FileObject Specified %wZ\n", &(targetFileObject->FileName));
      eventLength += targetFileObject->FileName.Length;
    }

    eventContext = AllocateEventContext(vcb->Dcb, Irp, eventLength, ccb);

    if (eventContext == NULL) {
      status = STATUS_INSUFFICIENT_RESOURCES;
      __leave;
    }

    eventContext->Context = ccb->UserContext;

    eventContext->Operation.SetFile.FileInformationClass =
        irpSp->Parameters.SetFile.FileInformationClass;

    // the size of FileInformation
    eventContext->Operation.SetFile.BufferLength =
        irpSp->Parameters.SetFile.Length;

    // the offset from begining of structure to fill FileInfo
    eventContext->Operation.SetFile.BufferOffset =
        FIELD_OFFSET(EVENT_CONTEXT, Operation.SetFile.FileName[0]) +
        fcb->FileName.Length + sizeof(WCHAR); // the last null char

    // copy FileInformation
    RtlCopyMemory(
        (PCHAR)eventContext + eventContext->Operation.SetFile.BufferOffset,
        Irp->AssociatedIrp.SystemBuffer, irpSp->Parameters.SetFile.Length);

    if (irpSp->Parameters.SetFile.FileInformationClass ==
            FileRenameInformation ||
        irpSp->Parameters.SetFile.FileInformationClass == FileLinkInformation) {
      // We need to hanle FileRenameInformation separetly because the structure
      // of FILE_RENAME_INFORMATION
      // has HANDLE type field, which size is different in 32 bit and 64 bit
      // environment.
      // This cases problems when driver is 64 bit and user mode library is 32
      // bit.
      PFILE_RENAME_INFORMATION renameInfo =
          (PFILE_RENAME_INFORMATION)Irp->AssociatedIrp.SystemBuffer;
      PDOKAN_RENAME_INFORMATION renameContext = (PDOKAN_RENAME_INFORMATION)(
          (PCHAR)eventContext + eventContext->Operation.SetFile.BufferOffset);

      // This code assumes FILE_RENAME_INFORMATION and FILE_LINK_INFORMATION
      // have
      // the same typse and fields.
      ASSERT(sizeof(FILE_RENAME_INFORMATION) == sizeof(FILE_LINK_INFORMATION));

      renameContext->ReplaceIfExists = renameInfo->ReplaceIfExists;
      renameContext->FileNameLength = renameInfo->FileNameLength;
      RtlCopyMemory(renameContext->FileName, renameInfo->FileName,
                    renameInfo->FileNameLength);

      if (targetFileObject != NULL) {
        // if Parameters.SetFile.FileObject is specified, replase
        // FILE_RENAME_INFO's file name by
        // FileObject's file name. The buffer size is already adjusted.
        DDbgPrint("  renameContext->FileNameLength %d\n",
                  renameContext->FileNameLength);
        DDbgPrint("  renameContext->FileName %ws\n", renameContext->FileName);
        RtlZeroMemory(renameContext->FileName, renameContext->FileNameLength);
        RtlCopyMemory(renameContext->FileName,
                      targetFileObject->FileName.Buffer,
                      targetFileObject->FileName.Length);
        renameContext->FileNameLength = targetFileObject->FileName.Length;
      }

      if (irpSp->Parameters.SetFile.FileInformationClass ==
          FileRenameInformation) {
        DDbgPrint("   rename: %wZ => %ls, FileCount = %u\n", fcb->FileName,
                  renameContext->FileName, (ULONG)fcb->FileCount);
      }
    }

    // copy the file name
    eventContext->Operation.SetFile.FileNameLength = fcb->FileName.Length;
    RtlCopyMemory(eventContext->Operation.SetFile.FileName,
                  fcb->FileName.Buffer, fcb->FileName.Length);

    status = FsRtlCheckOplock(DokanGetFcbOplock(fcb), Irp, eventContext,
                              DokanOplockComplete, DokanPrePostIrp);

    //
    //  if FsRtlCheckOplock returns STATUS_PENDING the IRP has been posted
    //  to service an oplock break and we need to leave now.
    //
    if (status != STATUS_SUCCESS) {
      if (status == STATUS_PENDING) {
        DDbgPrint("   FsRtlCheckOplock returned STATUS_PENDING\n");
      } else {
        DokanFreeEventContext(eventContext);
      }
      __leave;
    }

    // register this IRP to waiting IRP list and make it pending status
    status = DokanRegisterPendingIrp(DeviceObject, Irp, eventContext, 0);

  } __finally {
    if(fcb)
      DokanFCBUnlock(fcb);

    DokanCompleteIrpRequest(Irp, status, 0);

    DDbgPrint("<== DokanSetInformation\n");
  }

  return status;
}

VOID DokanCompleteSetInformation(__in PIRP_ENTRY IrpEntry,
                                 __in PEVENT_INFORMATION EventInfo) {
  PIRP irp;
  PIO_STACK_LOCATION irpSp;
  NTSTATUS status;
  ULONG info = 0;
  PDokanCCB ccb;
  PDokanFCB fcb = NULL;
  UNICODE_STRING oldFileName;

  FILE_INFORMATION_CLASS infoClass;
  irp = IrpEntry->Irp;
  status = EventInfo->Status;

  __try {

    DDbgPrint("==> DokanCompleteSetInformation\n");

    irpSp = IrpEntry->IrpSp;

    ccb = IrpEntry->FileObject->FsContext2;
    ASSERT(ccb != NULL);

    KeEnterCriticalRegion();
    ExAcquireResourceExclusiveLite(&ccb->Resource, TRUE);

    fcb = ccb->Fcb;
    ASSERT(fcb != NULL);
    DokanFCBLockRW(fcb);

    ccb->UserContext = EventInfo->Context;

    info = EventInfo->BufferLength;

    infoClass = irpSp->Parameters.SetFile.FileInformationClass;

    RtlZeroMemory(&oldFileName, sizeof(UNICODE_STRING));

    if (NT_SUCCESS(status)) {

      if (infoClass == FileDispositionInformation) {

        if (EventInfo->Operation.Delete.DeleteOnClose) {

          if (!MmFlushImageSection(&fcb->SectionObjectPointers,
                                   MmFlushForDelete)) {
            DDbgPrint("  Cannot delete user mapped image\n");
            status = STATUS_CANNOT_DELETE;
          } else {
            ccb->Flags |= DOKAN_DELETE_ON_CLOSE;
            fcb->Flags |= DOKAN_DELETE_ON_CLOSE;
            DDbgPrint("   FileObject->DeletePending = TRUE\n");
            IrpEntry->FileObject->DeletePending = TRUE;
          }

        } else {
          ccb->Flags &= ~DOKAN_DELETE_ON_CLOSE;
          fcb->Flags &= ~DOKAN_DELETE_ON_CLOSE;
          DDbgPrint("   FileObject->DeletePending = FALSE\n");
          IrpEntry->FileObject->DeletePending = FALSE;
        }
      }

      // if rename is executed, reassign the file name
      if (infoClass == FileRenameInformation) {
        PVOID buffer = NULL;

        // this is used to inform rename in the bellow switch case
        oldFileName.Buffer = fcb->FileName.Buffer;
        oldFileName.Length = (USHORT)fcb->FileName.Length;
        oldFileName.MaximumLength = (USHORT)fcb->FileName.Length;

        // copy new file name
        buffer = ExAllocatePool(EventInfo->BufferLength + sizeof(WCHAR));

        if (buffer == NULL) {
          status = STATUS_INSUFFICIENT_RESOURCES;
          ExReleaseResourceLite(&ccb->Resource);
          KeLeaveCriticalRegion();
          __leave;
        }

        fcb->FileName.Buffer = buffer;

        ASSERT(fcb->FileName.Buffer != NULL);

        RtlZeroMemory(fcb->FileName.Buffer,
                      EventInfo->BufferLength + sizeof(WCHAR));
        RtlCopyMemory(fcb->FileName.Buffer, EventInfo->Buffer,
                      EventInfo->BufferLength);

        fcb->FileName.Length = (USHORT)EventInfo->BufferLength;
        fcb->FileName.MaximumLength = (USHORT)EventInfo->BufferLength;

      }
    }

    ExReleaseResourceLite(&ccb->Resource);
    KeLeaveCriticalRegion();

    if (NT_SUCCESS(status)) {
      switch (irpSp->Parameters.SetFile.FileInformationClass) {
      case FileAllocationInformation:
        DokanNotifyReportChange(fcb, FILE_NOTIFY_CHANGE_SIZE,
                                FILE_ACTION_MODIFIED);
        break;
      case FileBasicInformation:
        DokanNotifyReportChange(fcb, FILE_NOTIFY_CHANGE_ATTRIBUTES |
                                         FILE_NOTIFY_CHANGE_LAST_WRITE |
                                         FILE_NOTIFY_CHANGE_LAST_ACCESS |
                                         FILE_NOTIFY_CHANGE_CREATION,
                                FILE_ACTION_MODIFIED);
        break;
      case FileDispositionInformation:
        if (IrpEntry->FileObject->DeletePending) {
          if (fcb->Flags & DOKAN_FILE_DIRECTORY) {
            DokanNotifyReportChange(fcb, FILE_NOTIFY_CHANGE_DIR_NAME,
                                    FILE_ACTION_REMOVED);
          } else {
            DokanNotifyReportChange(fcb, FILE_NOTIFY_CHANGE_FILE_NAME,
                                    FILE_ACTION_REMOVED);
          }
        }
        break;
      case FileEndOfFileInformation:
        DokanNotifyReportChange(fcb, FILE_NOTIFY_CHANGE_SIZE,
                                FILE_ACTION_MODIFIED);
        break;
      case FileLinkInformation:
        // TODO: should check whether this is a directory
        // TODO: should notify new link name
        // DokanNotifyReportChange(vcb, ccb, FILE_NOTIFY_CHANGE_FILE_NAME,
        // FILE_ACTION_ADDED);
        break;
      case FilePositionInformation:
        // this is never used
        break;
      case FileRenameInformation: {
        DokanNotifyReportChange0(fcb, &oldFileName,
                                 FILE_NOTIFY_CHANGE_FILE_NAME,
                                 FILE_ACTION_RENAMED_OLD_NAME);

        // free old file name
        ExFreePool(oldFileName.Buffer);

        DokanNotifyReportChange(fcb, FILE_NOTIFY_CHANGE_FILE_NAME,
                                FILE_ACTION_RENAMED_NEW_NAME);
      } break;
      case FileValidDataLengthInformation:
        DokanNotifyReportChange(fcb, FILE_NOTIFY_CHANGE_SIZE,
                                FILE_ACTION_MODIFIED);
        break;
      default:
        DDbgPrint("  unknown type:%d\n",
                  irpSp->Parameters.SetFile.FileInformationClass);
        break;
      }
    }

  } __finally {
    if(fcb)
      DokanFCBUnlock(fcb);

    DokanCompleteIrpRequest(irp, status, info);

    DDbgPrint("<== DokanCompleteSetInformation\n");
  }
}
