; ModuleID = 'tests/LICM/bench2_opt.bc'
source_filename = "tests/LICM/bench2.c"
target datalayout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-i128:128-f80:128-n8:16:32:64-S128"
target triple = "x86_64-unknown-linux-gnu"

; Function Attrs: noinline nounwind uwtable
define dso_local i32 @test(i32 noundef %0, i32 noundef %1, i32 noundef %2) #0 {
  %4 = add nsw i32 %0, %1
  %5 = mul nsw i32 %1, %2
  %6 = add nsw i32 %4, %5
  br label %7

7:                                                ; preds = %12, %3
  %.01 = phi i32 [ 0, %3 ], [ %11, %12 ]
  %.0 = phi i32 [ 0, %3 ], [ %13, %12 ]
  %8 = icmp slt i32 %.0, 1000
  br i1 %8, label %9, label %14

9:                                                ; preds = %7
  %10 = add nsw i32 %6, %.0
  %11 = add nsw i32 %.01, %10
  br label %12

12:                                               ; preds = %9
  %13 = add nsw i32 %.0, 1
  br label %7, !llvm.loop !6

14:                                               ; preds = %7
  ret i32 %.01
}

; Function Attrs: noinline nounwind uwtable
define dso_local i32 @main() #0 {
  %1 = call i32 @test(i32 noundef 2, i32 noundef 3, i32 noundef 4)
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
