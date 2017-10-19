CMAKE_MINIMUM_REQUIRED (VERSION 2.6)
PROJECT (liblmdb C)

ADD_LIBRARY(lmdb STATIC util_fns.c mempool.c lmlog.c fscanln.c lmdb.c lmfeature.c)

