
/**
 *  \file   common.h
 *  \brief  top level src include file
 *  \author Antoine Fraboulet
 *  \date   2011
 **/

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

enum wsim_end_mode_t {
  WSIM_END_NORMAL,
  WSIM_END_SIGNAL,
  WSIM_END_ERROR
};

/*
 * Start simulation main loop.
 */
int startWorker(int argc, char* argv[]);
void main_end(enum wsim_end_mode_t mode);

#ifdef __cplusplus
}
#endif