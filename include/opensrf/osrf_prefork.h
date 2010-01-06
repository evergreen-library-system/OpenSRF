#ifndef OSRF_PREFORK_H
#define OSRF_PREFORK_H

#ifdef __cplusplus
extern "C" {
#endif

/* we receive data.  we find the next child in
	line that is available.  pass the data down that childs pipe and go
	back to listening for more data.
	when we receive SIGCHLD, we check for any dead children and clean up
	their respective prefork_child objects, close pipes, etc.

	we build a select fd_set with all the child pipes (going to the parent) 
	when a child is done processing a request, it writes a small chunk of 
	data to the parent to alert the parent that the child is again available 
	*/

int osrf_prefork_run(const char* appname);

#ifdef __cplusplus
}
#endif

#endif
