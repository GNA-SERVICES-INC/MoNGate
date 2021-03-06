/* Test if ObjC constant string layout is checked properly, regardless of how
   constant string classes get derived.  */
/* Contributed by Ziemowit Laski <zlaski@apple.com>  */

/* { dg-do compile { target { *-*-darwin* } } } */
/* { dg-skip-if "" { *-*-* } { "-fgnu-runtime" } { "" } } */
/* { dg-options "-mno-constant-cfstrings" { target *-*-darwin* } } */

#include "../../../objc-obj-c++-shared/Object1.h"

@interface NSString: Object
@end

@interface NSSimpleCString : NSString {
@protected
    char *bytes;
    unsigned int numBytes;
}
@end
    
@interface NSConstantString : NSSimpleCString
@end

#ifndef NEXT_OBJC_USE_NEW_INTERFACE
extern struct objc_class _NSConstantStringClassReference;
#else
extern Class _NSConstantStringClassReference;
#endif

const NSConstantString *appKey = @"MyApp";

/* { dg-final { scan-assembler ".section __OBJC, __cstring_object" } } */
/* { dg-final { scan-assembler ".long\t__NSConstantStringClassReference\n\t.long\t.*\n\t.long\t5\n\t.data" { target { *-*-darwin* && { ! lp64 } } } } } */
/* { dg-final { scan-assembler ".quad\t__NSConstantStringClassReference\n\t.quad\t.*\n\t.long\t5\n\t.space" { target { *-*-darwin* && { lp64 } } } } } */
