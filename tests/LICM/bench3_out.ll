; ModuleID = 'tests/LICM/bench3_opt.bc'
source_filename = "tests/LICM/bench3.c"
target datalayout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-i128:128-f80:128-n8:16:32:64-S128"
target triple = "x86_64-unknown-linux-gnu"

; Function Attrs: noinline nounwind uwtable
define dso_local i32 @test(i32 noundef %0) #0 {
  %2 = mul nsw i32 %0, 2
  br label %3

3:                                                ; preds = %14, %1
  %.02 = phi i32 [ 0, %1 ], [ %15, %14 ]
  %.01 = phi i32 [ 0, %1 ], [ %.1, %14 ]
  %4 = icmp slt i32 %.02, 100
  br i1 %4, label %5, label %16

5:                                                ; preds = %3
  %6 = add nsw i32 %2, %.02
  br label %7

7:                                                ; preds = %11, %5
  %.1 = phi i32 [ %.01, %5 ], [ %10, %11 ]
  %.0 = phi i32 [ 0, %5 ], [ %12, %11 ]
  %8 = icmp slt i32 %.0, 100
  br i1 %8, label %9, label %13

9:                                                ; preds = %7
  %10 = add nsw i32 %.1, %6
  br label %11

11:                                               ; preds = %9
  %12 = add nsw i32 %.0, 1
  br label %7, !llvm.loop !6

13:                                               ; preds = %7
  br label %14

14:                                               ; preds = %13
  %15 = add nsw i32 %.02, 1
  br label %3, !llvm.loop !8

16:                                               ; preds = %3
  ret i32 %.01
}

; Function Attrs: noinline nounwind uwtable
define dso_local i32 @main() #0 {
  %1 = call i32 @test(i32 noundef 5)
  ret i32 %1
}

attributes #0 = { noinline nounwind uwtable "frame-pointer"="all" "min-legal-vector-width"="0" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "tune-cpu"="generic" }

!llvm.module.flags = !{!0, !1, !2, !3, !4}
!llvm.ident = !{!5}

!0 = !{i32 1, !"wchar_size", i32 4}
!1 = !{i32 8, !"PIC Level", i32 2}
!2 = !{i32 7, !"PIE Level", i32 2}
!3 = !{i32 7, !"uwtable", i32 2}
!4 = !{i32 7, !"frame-pointer", i32 2}
!5 = !{!"clang version 21.1.8 (https://github.com/llvm/llvm-project 2078da43e25a4623cab2d0d60decddf709aaea28)"}
!6 = distinct !{!6, !7}
!7 = !{!"llvm.loop.mustprogress"}
!8 = distinct !{!8, !7}
