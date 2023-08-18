/*****************************************************************************\
* iftMetrics.h
*
* AUTHOR  : Felipe Belem
* DATE    : 2021-03-08
* LICENSE : MIT License
* EMAIL   : felipe.belem@ic.unicamp.br
\*****************************************************************************/
#ifndef IFT_METRICS_H
#define IFT_METRICS_H

#ifdef __cplusplus
extern "C" {
#endif

#include "ift.h"

//############################################################################|
// 
//	PUBLIC METHODS
//
//############################################################################|
//============================================================================|
// Auxiliary
//============================================================================|
/*
 * Relabels the input label image to the interval [1,N] in which N is the 
 * total number of connected components in the label image (defined by an
 * 8- or 26-adjacency).
 *
 * PARAMETERS:
 *    label_img[in] - REQUIRED: Superspel segmentation
 *
 * RETURNS: Relabeled superspel segmentation
 */
iftImage *iftRelabelImage
(iftImage *label_img);

//============================================================================|
// Non-GT-based
//============================================================================|
/*
 * Evaluates the compacity of the superspel segmentation by comparing
 * their ratio to a circle/sphere of the same radius. Higher is better.
 *
 * Based on:
 * 	- A. Schick, M. Fischer, R. Stiefelhagen. Measuring and evaluating the 
 *		compactness of superpixels. 2012.
 *  - R. Yi, Y. -J. Liu and Y. -K. Lai. Evaluation on the Compactness of 
 *    Supervoxels, 2018.
 *
 * PARAMETERS:
 *		label_img[in] - REQUIRED: Superspel segmentation
 *
 * RETURNS: Compacity value between [0,1]
 */
float iftEvalCO
(iftImage *label_img);

/*
 * Evaluates the contour density of the superspel segmentation by calculating
 * the ratio of contours over the image's size. Lower is better.
 *	
 * Based on:
 * 	- V. Machairas, M. Faessel, D. Cardenas-Pena, T. Chabardes, T. Walter, 
 * 		E. Decenciere. Waterpixels. 2015.
 *
 * PARAMETERS:
 *		label_img[in] - REQUIRED: Superspel segmentation
 *
 * RETURNS: Contour density value between [0,1] 
 */
float iftEvalCD
(iftImage *label_img);

/*
 * Evaluates the explained variation of the superspel segmentation by 
 * comparing the superspel and the spel variance towards the image's mean.
 * Higher is better.
 * 
 * Based on:
 *	- A. P. Moore, S. J. D. Prince, J. Warrell, U. Mohammed, G. Jones. 
 *			Superpixel lattices. 2008.
 *
 * PARAMETERS:
 *	label_img[in] - REQUIRED: Superspel segmentation 
 *	orig_img[in] - REQUIRED: Original image
 *
 * RETURNS: Explained variation value between [0,1] 
 */
float iftEvalEV
(iftImage *label_img, iftImage *orig_img);

/*
 * Evaluates the temporal extension of the superspel segmentation through the
 * mean Z-axis percentual length of all superspels. Higher is better.
 * 
 * Based on:
 *  - J. Chang, D. Wei, J. W. Fisher III. A Video Representation Using 
 *    Temporal Superpixels, 2013.
 *
 * PARAMETERS:
 *  label_img[in] - REQUIRED: Superspel segmentation 
 *
 * RETURNS: Temporal extension value between [0,1] 
 */
float iftEvalTEX
(iftImage *label_img);

//============================================================================|
// GT-based
//============================================================================|
/*
 * Evaluates the achievable segmentation accuracy of the superspel
 * segmentation by estimating the best achievable segmentation with an ideal
 * selection of the superspels. Higher is better.
 * 
 * Based on:
 * 	- M. Y. Lui, O. Tuzel, S. Ramalingam, R. Chellappa. Entropy rate
 * 		superpixel segmentation. 2011.
 *
 * PARAMETERS:
 * 	label_img[in] - REQUIRED: Superspel segmentation
 * 	gt_img[in] - REQUIRED: Ground-truth
 *
 * RETURNS: Achievable segmentation accuracy value between [0,1] 
 */
float iftEvalASA
(iftImage *label_img, iftImage *gt_img);

/*
 * Evaluates the boundary recall of the superspel segmentation by calculating
 * the ratio of the borders overlapping the objects' boundaries. Higher is 
 * better.
 * 
 * Based on:
 * 	- D. Stutz, A. Hermans, B. Leibe. Superpixels: An Evaluation of the 
 * 		State-of-the-Art. 2018.
 *
 * PARAMETERS:
 * 	label_img[in] - REQUIRED: Superspel segmentation
 * 	gt_img[in] - REQUIRED: Ground-truth
 *
 * RETURNS: Boundary recall value between [0,1] 
 */
float iftEvalBR
(iftImage *label_img, iftImage *gt_img);

/*
 * Evaluates the under-segmentation error of the superspel segmentation by 
 * calculating the error of multiple object overlap by the superspels. Lower
 * is better.
 * 
 * Based on:
 * 	- P. Neubert, P. Protzel. Superpixel benchmark and comparison. 2012.
 *
 * PARAMETERS:
 * 	label_img[in] - REQUIRED: Superspel segmentation
 * 	gt_img[in] - REQUIRED: Ground-truth
 *
 * RETURNS: Under-segmentation error value between [0,1] 
 */
float iftEvalUE
(iftImage *label_img, iftImage *gt_img);

#ifdef __cplusplus
}
#endif

#endif
