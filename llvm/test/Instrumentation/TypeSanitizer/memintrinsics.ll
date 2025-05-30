; NOTE: Assertions have been autogenerated by utils/update_test_checks.py
; Test basic type sanitizer instrumentation.
;
; RUN: opt -passes='tysan' -S %s | FileCheck %s

target datalayout = "e-m:e-i64:64-f80:128-n8:16:32:64-S128"

declare void @llvm.memset.p0.i64(ptr nocapture, i8, i64, i32, i1) nounwind
declare void @llvm.memmove.p0.p0.i64(ptr nocapture, ptr nocapture readonly, i64, i32, i1) nounwind
declare void @llvm.memcpy.p0.p0.i64(ptr nocapture, ptr nocapture readonly, i64, i32, i1) nounwind

define void @test_memset(ptr %a, ptr %b) nounwind uwtable sanitize_type {
; CHECK-LABEL: @test_memset(
; CHECK-NEXT:  entry:
; CHECK-NEXT:    [[APP_MEM_MASK:%.*]] = load i64, ptr @__tysan_app_memory_mask, align 8
; CHECK-NEXT:    [[SHADOW_BASE:%.*]] = load i64, ptr @__tysan_shadow_memory_address, align 8
; CHECK-NEXT:    [[TMP0:%.*]] = ptrtoint ptr [[A:%.*]] to i64
; CHECK-NEXT:    [[TMP1:%.*]] = and i64 [[TMP0]], [[APP_MEM_MASK]]
; CHECK-NEXT:    [[TMP2:%.*]] = shl i64 [[TMP1]], 3
; CHECK-NEXT:    [[TMP3:%.*]] = add i64 [[TMP2]], [[SHADOW_BASE]]
; CHECK-NEXT:    [[TMP4:%.*]] = inttoptr i64 [[TMP3]] to ptr
; CHECK-NEXT:    call void @llvm.memset.p0.i64(ptr align 8 [[TMP4]], i8 0, i64 800, i1 false)
; CHECK-NEXT:    call void @llvm.memset.p0.i64(ptr align 1 [[A]], i8 0, i64 100, i1 false)
; CHECK-NEXT:    ret void
;
  entry:
  tail call void @llvm.memset.p0.i64(ptr %a, i8 0, i64 100, i32 1, i1 false)
  ret void
}

define void @test_memmove(ptr %a, ptr %b) nounwind uwtable sanitize_type {
; CHECK-LABEL: @test_memmove(
; CHECK-NEXT:  entry:
; CHECK-NEXT:    [[APP_MEM_MASK:%.*]] = load i64, ptr @__tysan_app_memory_mask, align 8
; CHECK-NEXT:    [[SHADOW_BASE:%.*]] = load i64, ptr @__tysan_shadow_memory_address, align 8
; CHECK-NEXT:    [[TMP0:%.*]] = ptrtoint ptr [[A:%.*]] to i64
; CHECK-NEXT:    [[TMP1:%.*]] = and i64 [[TMP0]], [[APP_MEM_MASK]]
; CHECK-NEXT:    [[TMP2:%.*]] = shl i64 [[TMP1]], 3
; CHECK-NEXT:    [[TMP3:%.*]] = add i64 [[TMP2]], [[SHADOW_BASE]]
; CHECK-NEXT:    [[TMP4:%.*]] = inttoptr i64 [[TMP3]] to ptr
; CHECK-NEXT:    [[TMP5:%.*]] = ptrtoint ptr [[B:%.*]] to i64
; CHECK-NEXT:    [[TMP6:%.*]] = and i64 [[TMP5]], [[APP_MEM_MASK]]
; CHECK-NEXT:    [[TMP7:%.*]] = shl i64 [[TMP6]], 3
; CHECK-NEXT:    [[TMP8:%.*]] = add i64 [[TMP7]], [[SHADOW_BASE]]
; CHECK-NEXT:    [[TMP9:%.*]] = inttoptr i64 [[TMP8]] to ptr
; CHECK-NEXT:    call void @llvm.memmove.p0.p0.i64(ptr align 8 [[TMP4]], ptr align 8 [[TMP9]], i64 800, i1 false)
; CHECK-NEXT:    call void @llvm.memmove.p0.p0.i64(ptr align 1 [[A]], ptr align 1 [[B]], i64 100, i1 false)
; CHECK-NEXT:    ret void
;
  entry:
  tail call void @llvm.memmove.p0.p0.i64(ptr %a, ptr %b, i64 100, i32 1, i1 false)
  ret void
}

define void @test_memcpy(ptr %a, ptr %b) nounwind uwtable sanitize_type {
; CHECK-LABEL: @test_memcpy(
; CHECK-NEXT:  entry:
; CHECK-NEXT:    [[APP_MEM_MASK:%.*]] = load i64, ptr @__tysan_app_memory_mask, align 8
; CHECK-NEXT:    [[SHADOW_BASE:%.*]] = load i64, ptr @__tysan_shadow_memory_address, align 8
; CHECK-NEXT:    [[TMP0:%.*]] = ptrtoint ptr [[A:%.*]] to i64
; CHECK-NEXT:    [[TMP1:%.*]] = and i64 [[TMP0]], [[APP_MEM_MASK]]
; CHECK-NEXT:    [[TMP2:%.*]] = shl i64 [[TMP1]], 3
; CHECK-NEXT:    [[TMP3:%.*]] = add i64 [[TMP2]], [[SHADOW_BASE]]
; CHECK-NEXT:    [[TMP4:%.*]] = inttoptr i64 [[TMP3]] to ptr
; CHECK-NEXT:    [[TMP5:%.*]] = ptrtoint ptr [[B:%.*]] to i64
; CHECK-NEXT:    [[TMP6:%.*]] = and i64 [[TMP5]], [[APP_MEM_MASK]]
; CHECK-NEXT:    [[TMP7:%.*]] = shl i64 [[TMP6]], 3
; CHECK-NEXT:    [[TMP8:%.*]] = add i64 [[TMP7]], [[SHADOW_BASE]]
; CHECK-NEXT:    [[TMP9:%.*]] = inttoptr i64 [[TMP8]] to ptr
; CHECK-NEXT:    call void @llvm.memcpy.p0.p0.i64(ptr align 8 [[TMP4]], ptr align 8 [[TMP9]], i64 800, i1 false)
; CHECK-NEXT:    call void @llvm.memcpy.p0.p0.i64(ptr align 1 [[A]], ptr align 1 [[B]], i64 100, i1 false)
; CHECK-NEXT:    ret void
;
  entry:
  tail call void @llvm.memcpy.p0.p0.i64(ptr %a, ptr %b, i64 100, i32 1, i1 false)
  ret void
}
