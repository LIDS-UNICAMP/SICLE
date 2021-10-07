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

/*****************************************************************************\
*
*                               PUBLIC FUNCTIONS
*
\*****************************************************************************/
//===========================================================================//
// NON GT-BASED
//===========================================================================//
/*
	Calculates the explained variation as presented by [1] and [2]. This metric
	is applicable to any image (2D, 3D or video)

	[1] - Xu, C., Corso, J. "LIBSVX: A Supervoxel Library and Benchmark for 
	Early Video Processing". International Journal of Computer Vision, 2016, 
	pp. 272–290
	[2] - D. Stutz, A. Hermans and B. Leibe. "Superpixels: An evaluation of the 
	state-of-the-art" Computer Vision and Image Understanding, 2018, pp. 1-27 
*/
float iftExplVaria
(const iftImage *orig_img, const iftImage *label_img);

/*
	Calculates the temporal extension of the superspels (i.e., supervoxels) as
	proposed by [1]. This metric is applicable only for video images.

	[1] - Xu, C., Corso, J. "LIBSVX: A Supervoxel Library and Benchmark for 
	Early Video Processing". International Journal of Computer Vision, 2016, 
	pp. 272–290
*/
float iftTempExt
(const iftImage *label_img);

//===========================================================================//
// GT-BASED
//===========================================================================//
/*
	Calculates the achievable segmentation accuracy as presented by [1]. This 
	metric is applicable to any image (2D, 3D or video)

	[1] - D. Stutz, A. Hermans and B. Leibe. "Superpixels: An evaluation of the 
	state-of-the-art" Computer Vision and Image Understanding, 2018, pp. 1-27 
*/
float iftAchiSegmAccur
(const iftImage *label_img, const iftImage *gt_img);

/*
	Calculates the boundary recall as presented by [1]. This metric is applicable
	 to any image (2D, 3D or video)

	[1] - D. Stutz, A. Hermans and B. Leibe. "Superpixels: An evaluation of the 
	state-of-the-art" Computer Vision and Image Understanding, 2018, pp. 1-27 
*/
float iftBoundRecall
(const iftImage *label_img, const iftImage *gt_img);

/*
	Calculates the under-segmentation error as proposed by [1]. This metric is 
	applicable to any image (2D, 3D or video)

	[1] - P. Neubert, P. Protzel, "Superpixel benchmark and comparison", Forum 
	Bildverarbeitung, 2012
*/
float iftUnderSegmError
(const iftImage *label_img, const iftImage *gt_img);

#ifdef __cplusplus
}
#endif

#endif