# -*- makefile -*-
#
# Copyright (c) 2001-2002 The Trustees of Indiana University.  
#                         All rights reserved.
# 
# This file is part of the Tsunami software package.  For license
# information, see the LICENSE file in the top level directory of the
# Tsunami source distribution.
#
# $Id: Makefile.am,v 1.1 2006/07/20 09:21:21 jwagnerhki Exp $
#

INCLUDES		= -I$(top_srcdir)/include

common_lib		= $(top_builddir)/util/libtsunami_common.a

bin_PROGRAMS		= readtest writetest
readtest_SOURCES	= readtest.c
#readtest_LDADD		= $(common_lib)
#readtest_DEPENDENCIES	= $(common_lib)

writetest_SOURCES	= writetest.c
#writetest_LDADD	= $(common_lib)
#writetest_DEPENDENCIES	= $(common_lib)
