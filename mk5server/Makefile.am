# -*- makefile -*-
#
# Copyright (c) 2001-2002 The Trustees of Indiana University.  
#                         All rights reserved.
# 
# This file is part of the Tsunami software package.  For license
# information, see the LICENSE file in the top level directory of the
# Tsunami source distribution.
#
# $Id: Makefile.am,v 1.2 2007/05/31 11:02:54 jwagnerhki Exp $
#

INCLUDES		= -I$(top_srcdir)/include

common_lib		= $(top_builddir)/common/libtsunami_common.a $(top_builddir)/common/libssapi.a -lm

bin_PROGRAMS		= mk5tsunamid

mk5tsunamid_SOURCES	= \
			config.c \
			io.c \
			log.c \
			main.c \
			network.c \
			protocol.c \
			transcript.c \
			mk5api.c \
			server.h
mk5tsunamid_LDADD	= $(common_lib)
mk5tsunamid_DEPENDENCIES	= $(common_lib)
