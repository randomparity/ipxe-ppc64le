
# Specify compressor
#
ZBIN		= $(ZBIN64)

# Assembler section type character
#
ASM_TCHAR	:= @
ASM_TCHAR_OPS	:= @

# PowerPC64 little-endian (ELFv2) specific flags
#
# Keep compiled code off the callee-saved floating-point and vector
# registers, since setjmp() does not save them.
#
CFLAGS		+= -msoft-float -mno-altivec -mno-vsx

# EFI requires -fshort-wchar, and nothing else currently uses wchar_t
#
CFLAGS		+= -fshort-wchar

# Include PowerPC64-specific headers
#
INCDIRS		:= arch/$(ARCH)/include $(INCDIRS)

# PowerPC64-specific directories containing source files
#
SRCDIRS		+= arch/ppc64le/core

# Include platform-specific Makefile
#
MAKEDEPS	+= arch/ppc64le/Makefile.$(PLATFORM)
include arch/ppc64le/Makefile.$(PLATFORM)
