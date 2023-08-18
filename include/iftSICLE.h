/*****************************************************************************\
* iftSICLE.h
*
* AUTHOR  : Felipe Belem
* DATE    : 2022-06-15
* LICENSE : MIT License
* EMAIL   : felipe.belem@ic.unicamp.br
\*****************************************************************************/
#ifndef IFT_SICLE_H
#define IFT_SICLE_H

#ifdef __cplusplus
extern "C" {
#endif

#include "ift.h"

//############################################################################|
// 
//	STRUCTS, ENUMS, UNIONS & TYPEDEFS
//
//############################################################################|
typedef enum ift_sicle_sampl
{
  IFT_SICLE_SAMPL_RND, // Random seed selection
  IFT_SICLE_SAMPL_GRID, // Grid seed selection
  IFT_SICLE_SAMPL_CUSTOM, // Custom relevance penalization
} iftSICLESampl;

typedef enum ift_sicle_pen
{
  IFT_SICLE_PEN_NONE, // No seed relevance penalization
  IFT_SICLE_PEN_OBJ, // Penalize if outside or far from object borders
  IFT_SICLE_PEN_BORD, // Penalize if far from object borders
  IFT_SICLE_PEN_OSB, // Penalize if outside and too close to adjacents 
  IFT_SICLE_PEN_BOBS, // Penalize if within object and too close to adjacents
  IFT_SICLE_PEN_CUSTOM, // Custom relevance penalization
} iftSICLEPen;

typedef enum ift_sicle_conn
{
	IFT_SICLE_CONN_FMAX, // Irregular
	IFT_SICLE_CONN_FSUM, // Boundary- and compacity-controlled
  IFT_SICLE_CONN_CUSTOM, // Custom connectivity function
} iftSICLEConn;

typedef enum ift_sicle_crit
{
  IFT_SICLE_CRIT_SIZE, // Size only
  IFT_SICLE_CRIT_MAXSC, // Size and maximum contrast
	IFT_SICLE_CRIT_MINSC, // Size and minimum contrast
	IFT_SICLE_CRIT_SPREAD, // Size and minimum adjacent distance
	IFT_SICLE_CRIT_CUSTOM, // Custom relevance function
} iftSICLECrit;

typedef struct ift_sicle_args
{
	bool use_diag; // Flag: use 8- or 26-neighborhood. Default: true
  bool use_dift; // Flag: use differential computation. Default: true
	int n0; // Initial quantity of seeds. Default: 3000
	int nf; // Final quantity of superspels. Default: 200
	int max_iters; // Maximum number of iterations for segmentation. Default: 5
	int adhr; // Fsum: Boundary adherence factor. Default: 12
  float irreg; // Fsum: Irregularity factor. Default: 0.12
  float alpha; // Saliency information importance. Default: 0
  iftIntArray *user_ni; // User-defined intermediary quantity of seeds.
  iftSICLESampl samplopt; // Option: Seed oversampling option: Default RND
	iftSICLEConn connopt; // Option: IFT connectivity function. Default: FMAX
	iftSICLECrit critopt; // Option: Seed removal criterion. Default: MINSC
  iftSICLEPen penopt; // Option: Seed relevance penalization. Default: NONE
} iftSICLEArgs;

typedef struct ift_sicle_alg iftSICLE;

//############################################################################|
// 
//	PUBLIC METHODS
//
//############################################################################|
//============================================================================|
// Constructors & Destructors
//============================================================================|
/*
 * Creates an instance with the default parametrization for SICLE.
 *
 * RETURNS: Default instance of the object
 */
iftSICLEArgs *iftCreateSICLEArgs();

/*
 * Creates an instance with respect to the input images provided. You may free
 * the inputs since they are copied to the structure.
 *
 * PARAMETERS:
 *	img[in] - REQUIRED: Original image to be segmented
 *	objsm[in] - OPTIONAL: Grayscale object saliency map
 *	mask[in] - OPTIONAL: Binary mask indication the region of interest
 *
 * RETURNS: SICLE prototype
 */
iftSICLE *iftCreateSICLE
(iftImage *img, iftImage *objsm, iftImage *mask);

/*
 * Deallocates the respective object 
 *
 * PARAMETERS:
 *	args[in/out] - REQUIRED: Pointer to the object to be free'd
 */
void iftDestroySICLEArgs
(iftSICLEArgs **args);

/*
 * Deallocates the respective object 
 *
 * PARAMETERS:
 *	sicle[in/out] - REQUIRED: Pointer to the object to be free'd
 */
void iftDestroySICLE
(iftSICLE **sicle);

//============================================================================|
// Runner
//============================================================================|
/*
 * Verifies whether the arguments are valid with respect to the SICLE prototype
 * provided. If yes, nothing happens; otherwise, an error is thrown. A set of
 * arguments is valid if:
 * 	1) N0 in ]2,|V|[; 
 *  2) Nf in [2, N0[; 
 *  3) Maximum number of iterations > 1; 
 * 	4) Compacity (Fsum) >= 0 ;
 *  5) Adherence (Fsum) >= 0 ;
 *  6) Alpha >= 0 ;
 *  7) Penalization should be none when no saliency is provided ;
 * 
 * PARAMETERS
 * 	sicle[in] - REQUIRED: SICLE prototype
 * 	args[in] - REQUIRED: SICLE arguments
 */
void iftVerifySICLEArgs
(iftSICLE *sicle, iftSICLEArgs *args);

/*
 * Runs the SICLE algorithm with the prototype and arguments provided, and 
 * returns a label image whose values are within [1,Nf], or [0,Nf] if a mask
 * was provided.
 * 
 * PARAMETERS:
 *  sicle[in] - REQUIRED: SICLE prototype
 *  args[in] - OPTIONAL: SICLE arguments
 *
 * RETURNS: Superspel segmentation whose labels are within [1,Nf] or [0,Nf]
 * 	if a mask was provided.
 */
iftImage *iftRunSICLE
(iftSICLE *sicle, iftSICLEArgs *args);

/*
 * Runs the SICLE algorithm with the prototype and arguments provided, and 
 * returns a multiscale label image whose values are within [1,Nf], or [0,Nf] 
 * if a mask was provided. The scales are ordered from the first iteration to
 * the last one.
 * 
 * PARAMETERS:
 *  sicle[in] - REQUIRED: SICLE prototype
 *  args[in] - OPTIONAL: SICLE arguments
 *  num_scales[out] - OPTIONAL: Number of scales generated
 *
 * RETURNS: Multiscale superspel segmentation whose labels are within [1,Nf] 
 *          or [0,Nf] if a mask was provided.
 */
iftImage **iftRunMultiscaleSICLE
(iftSICLE *sicle, iftSICLEArgs *args, int *num_scales);

#ifdef __cplusplus
}
#endif

#endif
