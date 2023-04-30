##
##  Makefile for Standard, Profile, Debug, and Release version of MiniSat
##

CSRCS     = $(wildcard *.c)
CHDRS     = $(wildcard *.h)
COBJS     = $(addsuffix .o, $(basename $(CSRCS)))

PCOBJS    = $(addsuffix p,  $(COBJS))
DCOBJS    = $(addsuffix d,  $(COBJS))
RCOBJS    = $(addsuffix r,  $(COBJS))

EXEC      = bdd_minisat_all

CC        = gcc
CFLAGS    = -std=c99
COPTIMIZE = -O3 -fomit-frame-pointer

.PHONY : s p d r build clean depend lib libd

s:	WAY=standard
p:	WAY=profile
d:	WAY=debug
r:	WAY=release
rs:	WAY=release static

# To convert OBDD into reduced BDD, uncomment the following lines concerning CUDD and also uncomment the line defining REDUCTION.
# Requirement: to do this, obtain CUDD version 2.5.0 and place it in the top directory.
#CUDD_DIR  = cudd-2.5.0
#CUDD_INCLUDE = -I$(CUDD_DIR)/include
#CUDD_LIB = $(CUDD_DIR)/dddmp/libdddmp.a $(CUDD_DIR)/cudd/libcudd.a $(CUDD_DIR)/mtr/libmtr.a \
#            $(CUDD_DIR)/st/libst.a $(CUDD_DIR)/util/libutil.a $(CUDD_DIR)/epd/libepd.a


# The solver can not count a huge number of solutions beyond a cerntain threshold.
# To count precisely, install the GNU MP bignum library and uncomment the following GMPFLAGS and define GMP in MYFLAGS.
GMPFLAGS =
#GMPFLAGS += -lgmp

MYFLAGS = 
### options for formula-BDD caching ###
MYFLAGS += -D LAZY				# Cache operations are reduced (Recommended).
MYFLAGS += -D CUTSETCACHE		# cutset is selected as formula-BDD caching. If this is not defined, separator is selected.
#######################################

### options for non-blocking solver ###
MYFLAGS += -D NONBLOCKING			# non-blocking procedure is selected as a base solver. If this is not defined, blocking procedure is selected.

# select at most one of the following backtrack methods: if none of them is selected, BJ+CBJ is selected.
#MYFLAGS	+= -D BT				# Chronological backtracking
MYFLAGS	+= -D BJ				# Non-chronological backtracking with level limit
#MYFLAGS	+= -D CBJ				# Conflict-directed Back Jumping

# if DLEVEL is not defined, sublevel-based first UIP scheme is selected.
 MYFLAGS += -D DLEVEL	# decision level-based first UIP scheme

#MYFLAGS += -D REFRESH		# refresh option in command line is enabled. If the number of BDD nodes exceeds a specified threshold, all solutions are dumpted to a file (if output file is specified in command line), all caches are refreshed, and search is continued.
#######################################

#MYFLAGS += -D TRIE_REC	# Recursive version of trie implementation. If this is not defined, iterative version is used.
#MYFLAGS += -D GMP				# GNU MP bignum library is used to count solutions.
#MYFLAGS += -D REDUCTION		# Reduction of compiled OBDD into fully reduced one is performed using CUDD library (Optional).

s:	CFLAGS+=$(COPTIMIZE) -ggdb -D NDEBUG $(MYFLAGS) -D VERBOSEDEBUG
p:	CFLAGS+=$(COPTIMIZE) -pg -ggdb -D DEBUG $(MYFLAGS)
d:	CFLAGS+=-O0 -ggdb -D DEBUG $(MYFLAGS)
r:	CFLAGS+=$(COPTIMIZE) -D NDEBUG $(MYFLAGS)
rs:	CFLAGS+=$(COPTIMIZE) -D NDEBUG $(MYFLAGS)

s:	build $(EXEC)
p:	build $(EXEC)_profile
d:	build $(EXEC)_debug
r:	build $(EXEC)_release
rs:	build $(EXEC)_static

build:
	@echo Building $(EXEC) "("$(WAY)")"

clean:
	@rm -f $(EXEC) $(EXEC)_profile $(EXEC)_debug $(EXEC)_release $(EXEC)_static \
	  $(COBJS) $(PCOBJS) $(DCOBJS) $(RCOBJS) depend.mak

## Build rule
%.o %.op %.od %.or:	%.c
	@echo Compiling: $<
	@$(CC) $(CFLAGS) $(CUDD_INCLUDE) -c -o $@ $<

## Linking rules (standard/profile/debug/release)
$(EXEC): $(COBJS)
	@echo Linking $(EXEC)
	@$(CC) $(COBJS) $(CUDD_LIB) $(GMPFLAGS) -lz -lm -ggdb -Wall -o $@ 

$(EXEC)_profile: $(PCOBJS)
	@echo Linking $@
	@$(CC) $(PCOBJS) $(CUDD_LIB) $(GMPFLAGS) -lz -lm -ggdb -Wall -pg -o $@

$(EXEC)_debug:	$(DCOBJS)
	@echo Linking $@
	@$(CC) $(DCOBJS) $(CUDD_LIB) $(GMPFLAGS) -lz -lm -ggdb -Wall -o $@

$(EXEC)_release: $(RCOBJS)
	@echo Linking $@
	@$(CC) $(RCOBJS) $(CUDD_LIB) $(GMPFLAGS) -lz -lm -Wall -o $@

$(EXEC)_static: $(RCOBJS)
	@echo Linking $@
	@$(CC) --static $(RCOBJS) $(CUDD_LIB) $(GMPFLAGS) -lz -lm -Wall -o $@

lib:	libbdd_minisat_all.a
libd:	libbdd_minisatd_all.a

libbdd_minisat_all.a:	solver.or csolver.or
	@echo Library: "$@ ( $^ )"
	@rm -f $@
	@ar cq $@ $^

libbdd_minisatd_all.a:	solver.od csolver.od
	@echo Library: "$@ ( $^ )"
	@rm -f $@
	@ar cq $@ $^


## Make dependencies
depend:	depend.mak
depend.mak:	$(CSRCS) $(CHDRS)
	@echo Making dependencies ...
	@$(CC) -MM $(CSRCS) > depend.mak
	@cp depend.mak /tmp/depend.mak.tmp
	@sed "s/o:/op:/" /tmp/depend.mak.tmp >> depend.mak
	@sed "s/o:/od:/" /tmp/depend.mak.tmp >> depend.mak
	@sed "s/o:/or:/" /tmp/depend.mak.tmp >> depend.mak
	@rm /tmp/depend.mak.tmp

include depend.mak
