/*****************************************************************************\
* iftArgs.h
*
* AUTHOR  : Felipe Belem
* DATE    : 2021-02-24
* LICENSE : MIT License
* EMAIL   : felipe.belem@ic.unicamp.br
\*****************************************************************************/
#ifndef IFT_ARGS_H
#define IFT_ARGS_H

#ifdef __cplusplus
extern "C" {
#endif

#include "ift.h"

//############################################################################|
// 
//  STRUCTS, ENUMS, UNIONS & TYPEDEFS
//
//############################################################################|
typedef struct ift_args iftArgs;

//############################################################################|
// 
//  PUBLIC METHODS
//
//############################################################################|
//============================================================================|
// Constructors & Destructors
//============================================================================|
/*
 * Creates an instance containing a reference to the array of arguments given
 * in parameter.
 *
 * PARAMETERS:
 *		argc[in] - REQUIRED: Number of arguments
 *		argv[in] - REQUIRED: Argument array from command-line
 *
 * RETURNS: Instance of the object
 */
iftArgs *iftCreateArgs
(const int argc, const char **argv);

/*
 * Deallocates the respective object 
 *
 * PARAMETERS:
 *		args[in/out] - REQUIRED: Pointer to the object to be free'd
 */
void iftDestroyArgs
(iftArgs **args);

//============================================================================|
// Getters & Setters & Verifiers
//============================================================================|
/*
 * Gets the string value associated to the argument token provided. That is,
 * the string at i+1 given the token at i. The token MUST NOT contain the
 * default prefix "--".
 *
 * PARAMETERS:
 *		args[in] - REQUIRED: Program's arguments 
 *		token[in] - REQUIRED: Token (sans prefix)
 *
 * RETURNS: A string, if the token was found; NULL, otherwise. 
 */
const char *iftGetArg
(const iftArgs *args, const char *token);

/*
 * Verifies if the token exists within the program's arguments. The token MUST
 * NOT contain the default prefix "--".
 *
 * PARAMETERS:
 *		args[in] - REQUIRED: Program's arguments 
 *		token[in] - REQUIRED: Token (sans prefix)
 *
 * RETURNS: True, if the token exists; false, otherwise.
 */
bool iftExistArg
(const iftArgs *args, const char *token);

/*
 * Verifies whether the token provided has a value associated to it. That is,
 * if exists a string at i+1 given the token at i. The token MUST NOT contain
 * the default prefix "--".
 *
 * PARAMETERS:
 *		args[in] - REQUIRED: Program's arguments 
 *		token[in] - REQUIRED: Token (sans prefix)
 *
 * RETURNS: True, if the token has a value; false, otherwise.
 */
bool iftHasArgVal
(const iftArgs *args, const char *token);

#ifdef __cplusplus
}
#endif

#endif
