//===- Objcopy.cpp --------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "llvm/ObjCopy/ObjCopy.h"
#include "llvm/ObjCopy/COFF/COFFConfig.h"
#include "llvm/ObjCopy/COFF/COFFObjcopy.h"
#include "llvm/ObjCopy/DXContainer/DXContainerConfig.h"
#include "llvm/ObjCopy/DXContainer/DXContainerObjcopy.h"
#include "llvm/ObjCopy/ELF/ELFConfig.h"
#include "llvm/ObjCopy/ELF/ELFObjcopy.h"
#include "llvm/ObjCopy/MachO/MachOConfig.h"
#include "llvm/ObjCopy/MachO/MachOObjcopy.h"
#include "llvm/ObjCopy/MultiFormatConfig.h"
#include "llvm/ObjCopy/XCOFF/XCOFFConfig.h"
#include "llvm/ObjCopy/XCOFF/XCOFFObjcopy.h"
#include "llvm/ObjCopy/wasm/WasmConfig.h"
#include "llvm/ObjCopy/wasm/WasmObjcopy.h"
#include "llvm/Object/AlternativeTraits.h"
#include "llvm/Object/COFF.h"
#include "llvm/Object/DXContainer.h"
#include "llvm/Object/ELFObjectFile.h"
#include "llvm/Object/Error.h"
#include "llvm/Object/MachO.h"
#include "llvm/Object/MachOUniversal.h"
#include "llvm/Object/Wasm.h"
#include "llvm/Object/XCOFFObjectFile.h"

using namespace llvm;
using namespace llvm::object;

StringRef objcopy::getObjectFormatName(const object::Binary &B) {
  if (const auto *OF = dyn_cast<ObjectFile>(&B))
    return OF->getFileFormatName();
  return {};
}

void objcopy::printCopyMessage(StringRef InPath, StringRef InFormatName,
                               StringRef OutPath, StringRef OutFormatName) {
  if (OutFormatName.empty())
    OutFormatName = InFormatName;
  outs() << "copy from '" << InPath << "' [" << InFormatName << "] to '"
         << OutPath << "' [" << OutFormatName << "]\n";
}

/// The function executeObjcopyOnBinary does the dispatch based on the format
/// of the input binary (ELF, MachO or COFF).
Error objcopy::executeObjcopyOnBinary(const MultiFormatConfig &Config,
                                      object::Binary &In, raw_ostream &Out) {
  match (In) {
    case { object::ELFObjectFileBase &ELFBinary } => {
      auto ELFCfg = Config.getELFConfig();
      if (!ELFCfg)
        return ELFCfg.takeError();

      return elf::executeObjcopyOnBinary(Config.getCommonConfig(), *ELFCfg,
                                         ELFBinary, Out);
    }
    case { object::COFFObjectFile &COFFBinary } => {
      auto COFFCfg = Config.getCOFFConfig();
      if (!COFFCfg)
        return COFFCfg.takeError();

      return coff::executeObjcopyOnBinary(Config.getCommonConfig(), *COFFCfg,
                                          COFFBinary, Out);
    }
    case { object::MachOObjectFile &MachOBinary } => {
      auto MachOCfg = Config.getMachOConfig();
      if (!MachOCfg)
        return MachOCfg.takeError();

      return macho::executeObjcopyOnBinary(
          Config.getCommonConfig(), *MachOCfg, MachOBinary, Out);
    }
    case { object::MachOUniversalBinary &MachOUniversalBinary } =>
      return macho::executeObjcopyOnMachOUniversalBinary(
          Config, MachOUniversalBinary, Out);
    case { object::WasmObjectFile &WasmBinary } => {
      auto WasmCfg = Config.getWasmConfig();
      if (!WasmCfg)
        return WasmCfg.takeError();

      return objcopy::wasm::executeObjcopyOnBinary(
          Config.getCommonConfig(), *WasmCfg, WasmBinary, Out);
    }
    case { object::XCOFFObjectFile &XCOFFBinary } => {
      auto XCOFFCfg = Config.getXCOFFConfig();
      if (!XCOFFCfg)
        return XCOFFCfg.takeError();

      return xcoff::executeObjcopyOnBinary(
          Config.getCommonConfig(), *XCOFFCfg, XCOFFBinary, Out);
    }
    case { object::DXContainerObjectFile &DXContainerBinary } => {
      auto DXContainerCfg = Config.getDXContainerConfig();
      if (!DXContainerCfg)
        return DXContainerCfg.takeError();

      return dxbc::executeObjcopyOnBinary(
          Config.getCommonConfig(), *DXContainerCfg, DXContainerBinary, Out);
    }
    case _ =>
      return createStringError(object_error::invalid_file_type,
                               "unsupported object file format");
  }
}
