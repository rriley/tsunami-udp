# -*- makefile -*-
#
# Copyright (c) 2001-2002 The Trustees of Indiana University.  
#                         All rights reserved.
# 
# This file is part of the Tsunami software package.  For license
# information, see the LICENSE file in the top level directory of the
# Tsunami source distribution.
#
# $Id: Makefile.am,v 1.2 2006/10/18 07:31:47 jwagnerhki Exp $
#

INCLUDES		= -I$(top_srcdir)/include

common_lib		= $(top_builddir)/common/libtsunami_common.a -lm

bin_PROGRAMS		= tsunamid

tsunamid_SOURCES	= \
			config.c \
			io.c \
			log.c \
			main.c \
			network.c \
			protocol.c \
			transcript.c \
			vsibctl.c \
			server.h
tsunamid_LDADD		= $(common_lib)
tsunamid_DEPENDENCIES	= $(common_lib)
