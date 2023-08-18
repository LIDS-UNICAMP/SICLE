/*****************************************************************************\
* iftSICLE.c
*
* AUTHOR  : Felipe Belem
* DATE    : 2022-06-15
* LICENSE : MIT License
* EMAIL   : felipe.belem@ic.unicamp.br
\*****************************************************************************/
#include "iftSICLE.h"

//############################################################################|
// 
//	MACROS
//
//############################################################################|
#define IFTSICLE_NIL IFT_INFINITY_INT_NEG // Temporary nil predecessor
#define IFTSICLE_BKGCOST IFT_INFINITY_DBL_NEG // Impede conquering
#define IFTSICLE_TMPCOST IFT_INFINITY_DBL // Temporary cost

// Encapsulate for readability
#define iftSICLE_InROI(sicle,v_index) \
	((sicle)->roi == NULL || iftBMapValue((sicle)->roi, (v_index)))
// Exploit the unused seed's predecessor for storing its label (2's complement)
#define iftSICLE_GetRootLabel(data,v_index) \
	(-(((data)->pred_map[(data)->root_map[(v_index)]]) + 1))

//############################################################################|
// 
//	STRUCTS, ENUMS, UNIONS & TYPEDEFS
//
//############################################################################|
struct ift_sicle_alg
{
	float *sal; // Spel saliency 
	iftMImage *mimg; // Spel features
	iftBMap *roi; // Bit-map region of interest (ROI)
};

typedef struct _iftsicle_iftdata
{
	int num_vtx; // Number of vertices
	int *root_map; // Root map
	int *pred_map; // "Predecessor and label" map for memory efficiency.
	double *cost_map; // Cost map
	iftIntArray *seeds; // Seeds at current iteration 
	iftAdjRel *A; // Adjacency relation
} iftSICLE_IFTData;

typedef struct _iftsicle_tstats
{
  int num_trees; // Number of trees/superspels
  int num_dims; // Number of (spatial) dimensions
  int num_feats; // Number of features
  int *size; // Tree's/superspel's size
  float *sal; // Tree's/superspel's mean saliency
  float **feats; // Tree's/superspel's mean features
  float **centr; // Tree's/superspel's centroid
  iftBMap **adj; // Tree's/superspel's adjacents
} iftSICLE_TStats;

//############################################################################|
// 
//	PRIVATE METHODS
//
//############################################################################|
//===========================================================================//
// General & Auxiliary
//===========================================================================//
/*
 * Creates the array containing the values of Ni at each iteration of SICLE.
 * If the user provides such values, then the array will contain N0, Nf and 
 * the user-defined quantities.
 *
 * PARAMETERS:
 *  args[in] - REQUIRED: SICLE arguments
 *
 * RETURNS: Array containing the values of Nf throughout the execution
 */
iftIntArray *iftSICLE_CreateNiArray
(iftSICLEArgs *args, iftSICLE_IFTData *data)
{
	int num_iters, real_n0;
	float omega;
	iftIntArray *ni;

	real_n0 = data->seeds->n;
	omega = 1.0/((float)args->max_iters - 1); // Exponential decay

	if(args->user_ni == NULL)
	{
		float approx;
		#ifdef IFT_DEBUG //-------------------------------------------------------|
		fprintf(stderr, "DEBUG (%s): omega = %f\n", __func__, omega);
		#endif //-----------------------------------------------------------------|
		approx = iftLog(real_n0/(float)args->nf, pow(real_n0, omega));
		num_iters = ceil(approx) + 1; // +1 for the last iteration
	}
	else
	{ num_iters = args->user_ni->n + 2; } // +2 for the first and last iterations

	ni = iftCreateIntArray(num_iters);
	ni->val[0] = real_n0; ni->val[num_iters - 1] = args->nf;
	for(long i = 1; i < num_iters - 1; ++i)
	{
		if(args->user_ni == NULL) 
		{ ni->val[i] = iftRound(pow(real_n0, 1.0-(omega*i))); }
		else
		{ ni->val[i] = args->user_ni->val[i - 1]; }
	}
	#ifdef IFT_DEBUG //---------------------------------------------------------|
	fprintf(stderr, "DEBUG (%s): Num Iters = %d\n", __func__, num_iters);
	#endif //-------------------------------------------------------------------|

	return ni;
}

//============================================================================|
// Output
//============================================================================|
/* 
 * Creates a label image from the IFT root map whose labels are within [1,N],
 * if no mask was provided; or [0,N] otherwise, being 0 for the background
 *
 * PARAMETERS:
 *  sicle[in] - REQUIRED: SICLE auxiliary data
 *  data[in] - REQUIRED: IFT auxiliary data
 *
 * RETURNS: Grayscale label image, whose labels are within [1,N] or [0,N]
 */
iftImage *iftSICLE_CreateLabelImage
(iftSICLE *sicle, iftSICLE_IFTData *data)
{
	iftImage *label_img;

	label_img = iftCreateImage(sicle->mimg->xsize, sicle->mimg->ysize, 
															sicle->mimg->zsize);
	#ifdef IFT_OMP //-----------------------------------------------------------|
	#pragma omp parallel for
	#endif //-------------------------------------------------------------------|
	for(int v_index = 0; v_index < sicle->mimg->n; ++v_index)
	{
		if(iftSICLE_InROI(sicle, v_index))
		{ label_img->val[v_index] = iftSICLE_GetRootLabel(data, v_index) + 1; }
	}
	return label_img;
}

/* 
 * Creates a seed label image from the seeds and from the IFT root map whose 
 * labels are within [1,N] at the seed's spels; and 0 otherwise
 *
 * PARAMETERS:
 *  sicle[in] - REQUIRED: SICLE auxiliary data
 *  data[in] - REQUIRED: IFT auxiliary data
 *
 * RETURNS: Grayscale label image, whose seeds' labels are within [1,N] at
 *			 		their spels
 */
iftImage *iftSICLE_CreateSeedImage
(iftSICLE *sicle, iftSICLE_IFTData *data)
{
	iftImage *seed_img;

	seed_img = iftCreateImage(sicle->mimg->xsize, sicle->mimg->ysize, 
														sicle->mimg->zsize);

	#ifdef IFT_OMP //-----------------------------------------------------------|
	#pragma omp parallel for
	#endif //-------------------------------------------------------------------|
	for(int s_id = 0; s_id < data->seeds->n; ++s_id)
	{
		int s_index;

		s_index = data->seeds->val[s_id];
		// Add +1 because 0 is for non-seed spels
		seed_img->val[s_index] = iftSICLE_GetRootLabel(data, s_index) + 1;
	}

	return seed_img;
}

//============================================================================|
// Seed Oversampling
//============================================================================|
/* 
 * Selects N0 seeds in a grid-like pattern throughout the image or limited to
 * the area delimited by the provided mask.
 *
 * PARAMETERS:
 *  sicle[in] - REQUIRED: SICLE auxiliary data
 *  args[in] - REQUIRED: SICLE arguments
 *
 * RETURNS: Array of N0 seed spel indexes
 */
iftIntArray *iftSICLE_GridOversampl
(iftSICLE *sicle, iftSICLEArgs *args)
{
	bool is3d;
  int x0, xf, y0, yf, z0, zf, all_length;
  float xstride, ystride, zstride, c, p_x, p_y, p_z;
  iftSet *tmp_seeds;
  iftIntArray *seeds;

  all_length = sicle->mimg->xsize + sicle->mimg->ysize + sicle->mimg->zsize;
  p_x = sicle->mimg->xsize / (float)all_length;
  p_y = sicle->mimg->ysize / (float)all_length;
  p_z = sicle->mimg->zsize / (float)all_length;

 	is3d = iftIs3DMImage(sicle->mimg);
  if(is3d){ c = (int)pow(args->n0/(p_x*p_y*p_z), 1.0/3.0); }
	else{ c = (int)sqrtf(args->n0/(p_x*p_y)); }
  
  xstride = sicle->mimg->xsize/(c * p_x);
  ystride = sicle->mimg->ysize/(c * p_y);
  zstride = sicle->mimg->zsize/(c * p_z);

  if(xstride < 1.0 || ystride < 1.0 || (zstride < 1.0 && is3d)) 
  { iftError("Excessive number of seeds!", __func__); }

  x0 = (int)(xstride/2.0); xf = sicle->mimg->xsize - 1;
  y0 = (int)(ystride/2.0); yf = sicle->mimg->ysize - 1;

  if(is3d){ z0 = (int)(zstride/2.0); zf = sicle->mimg->zsize - 1; }
	else { z0 = zf = 0; } // Dismiss the z stride

  tmp_seeds = NULL;
  for(int z = z0; z <= zf; z = (int)(z + zstride))
  {
	  for(int y = y0; y <= yf; y = (int)(y + ystride))
	  {
	    for(int x = x0; x <= xf; x = (int)(x + xstride))
	    {
	      int s_index;
      	iftVoxel s_voxel;

      	s_voxel.x = x; s_voxel.y = y; s_voxel.z = z;
      	s_index = iftMGetVoxelIndex(sicle->mimg, s_voxel);

      	// If falls outside mask, do not add as seed and move on
      	if(iftSICLE_InROI(sicle, s_index))
      	{ iftInsertSet(&tmp_seeds, s_index); }
	  	}
	  }
	}

  seeds = iftSetToArray(tmp_seeds);
  iftDestroySet(&tmp_seeds);

  return seeds;
}

/* 
 * Selects N0 random seeds throughout the image or limited to the area 
 * delimited by the provided mask.
 *
 * PARAMETERS:
 *  sicle[in] - REQUIRED: SICLE auxiliary data
 *  args[in] - REQUIRED: SICLE arguments
 *
 * RETURNS: Array of N0 seed spel indexes
 */
iftIntArray *iftSICLE_RndOversampl
(iftSICLE *sicle, iftSICLEArgs *args)
{
	int num_sampled;
	iftBMap *marked;
	iftIntArray *seeds;

	seeds = iftCreateIntArray(args->n0);
	marked = iftCreateBMap(sicle->mimg->n);

	num_sampled = 0;
	while(num_sampled < args->n0)
	{
		int s_index;

		s_index = iftRandomInteger(0, sicle->mimg->n - 1);
		if(!iftBMapValue(marked, s_index) && iftSICLE_InROI(sicle, s_index))
		{ // Was not selected and is non-masked?
		  seeds->val[num_sampled] = s_index;
		  iftBMapSet1(marked, s_index); // Mark as selected
		  num_sampled++;
		}
	}
	iftDestroyBMap(&marked);

	return seeds;
}

//============================================================================|
// iftSICLE_IFTData
//============================================================================|
/*
 * Creates and allocates memory for an IFT data instance
 *
 * PARAMETERS:
 *  sicle[in] - REQUIRED: SICLE auxiliary data
 *  args[in] - REQUIRED: SICLE arguments
 *
 * RETURNS: Instance of the object
 */
iftSICLE_IFTData *iftSICLE_CreateIFTData
(iftSICLE *sicle, iftSICLEArgs *args)
{
	iftSICLE_IFTData *data;

	data = malloc(sizeof(iftSICLE_IFTData));
	assert(data != NULL);

	data->num_vtx = sicle->mimg->n;
	data->root_map = calloc(sicle->mimg->n, sizeof(int));
	assert(data->root_map != NULL);
	data->pred_map = calloc(sicle->mimg->n, sizeof(int));
	assert(data->pred_map != NULL);
	data->cost_map = calloc(sicle->mimg->n, sizeof(double));
	assert(data->cost_map != NULL);
	
	if(args->use_diag == true) 
	{
		if(iftIs3DMImage(sicle->mimg) == true) 
		{ data->A = iftSpheric(sqrtf(3.0)); }
		else { data->A = iftCircular(sqrtf(2.0)); }
	}
	else
	{
		if(iftIs3DMImage(sicle->mimg) == true) { data->A = iftSpheric(1.0); }
		else { data->A = iftCircular(1.0); }
	}

	if(args->samplopt == IFT_SICLE_SAMPL_RND)
	{ data->seeds = iftSICLE_RndOversampl(sicle, args); }
	else if(args->samplopt == IFT_SICLE_SAMPL_GRID)
	{ data->seeds = iftSICLE_GridOversampl(sicle, args); }
	else if(args->samplopt == IFT_SICLE_SAMPL_CUSTOM)
	{ 
  	/* 
			You may write here your own seed oversampling method for testing possible
			new functionalities for SICLE. For using that, add "--sampl-opt custom"
			in the command-line
		*/
		data->seeds = iftSICLE_RndOversampl(sicle, args); 
	}
	else
	{ iftError("Unknown seed sampling option", __func__); }

	#ifdef IFT_DEBUG //---------------------------------------------------------|
	fprintf(stderr, "DEBUG (%s): %ld seeds sampled\n", __func__, data->seeds->n);
	#endif //-------------------------------------------------------------------|	

	return data;
}

/*
 * Deallocates the respective object 
 *
 * PARAMETERS:
 *		args[in/out] - REQUIRED: Pointer to the object to be free'd
 */
void iftSICLE_DestroyIFTData
(iftSICLE_IFTData **data)
{
	free((*data)->root_map);
	free((*data)->pred_map);
	free((*data)->cost_map);
	iftDestroyIntArray(&((*data)->seeds));
	iftDestroyAdjRel(&((*data)->A));

	free(*data);
	(*data) = NULL;
}

/*
 * Resets the IFT data to a pre-IFT state: assigns temporary costs and 
 * predecessors, and reassigns labels to seeds and background spels.
 *
 * PARAMETERS:
 *  sicle[in] - REQUIRED: SICLE auxiliary data
 *  data[in/out] - REQUIRED: IFT auxiliary data to be modified
 */
void iftSICLE_ResetIFTData
(iftSICLE *sicle, iftSICLE_IFTData **data)
{
	#ifdef IFT_OMP //-----------------------------------------------------------|
	#pragma omp parallel for
	#endif //-------------------------------------------------------------------|
	for(int v_index = 0; v_index < sicle->mimg->n; ++v_index)
	{
		(*data)->pred_map[v_index] = IFTSICLE_NIL;
		(*data)->root_map[v_index] = IFTSICLE_NIL;

		if(!iftSICLE_InROI(sicle,v_index)) // Won't be conquered
		{ (*data)->cost_map[v_index] = IFTSICLE_BKGCOST; } 
		else 
		{ (*data)->cost_map[v_index] = IFTSICLE_TMPCOST; }
	}

	#ifdef IFT_OMP //-----------------------------------------------------------|
	#pragma omp parallel for
	#endif //-------------------------------------------------------------------|
	for(int s_id = 0; s_id < (*data)->seeds->n; ++s_id)
	{
		int s_index;

		s_index = (*data)->seeds->val[s_id];
		(*data)->root_map[s_index] = s_index;
		(*data)->pred_map[s_index] = -(s_id + 1); // 2's complement
		(*data)->cost_map[s_index] = 0;
	}
}

/*
 * Removes the trees of the irrelevant seeds, marked to be removed, and 
 * returns the spels at the frontier to be inserted for the differential 
 * computation.
 *
 * PARAMETERS:
 *  sicle[in] - REQUIRED: SICLE auxiliary data
 *  data[in/out] - REQUIRED: IFT auxiliary data to be modified
 *  irre_seeds[in/out] - REQUIRED: Irrelevant seeds to be removed
 *
 * RETURNS: Spels at the frontier of the removed trees
 */
iftSet *iftSICLE_RemoveTrees
(iftSICLE *sicle, iftSICLE_IFTData **data, iftSet **irre_seeds)
{
	iftBMap *marked;
  iftSet *frontier, *remove;

  marked = iftCreateBMap((*data)->num_vtx);

  remove = NULL;
  while((*irre_seeds) != NULL)
  {
  	int s_index;

  	s_index = iftRemoveSet(irre_seeds);
		(*data)->pred_map[s_index] = IFTSICLE_NIL; // Temporary predecessor		
    (*data)->root_map[s_index] = IFTSICLE_NIL; // Temporary root
    (*data)->cost_map[s_index] = IFTSICLE_TMPCOST; // Temporary cost
    iftInsertSet(&remove, s_index); // Add for BFS removal
  }

  frontier = NULL;
  while(remove != NULL)
  {
  	int vi_index;
  	iftVoxel vi_voxel;

  	vi_index = iftRemoveSet(&remove);
  	vi_voxel = iftMGetVoxelCoord(sicle->mimg, vi_index);

  	for(int j = 1; j < (*data)->A->n; ++j)
  	{
  		iftVoxel vj_voxel;

  		vj_voxel = iftGetAdjacentVoxel((*data)->A, vi_voxel, j);

  		if(iftMValidVoxel(sicle->mimg, vj_voxel))
  		{
  			int vj_index, vj_root;

  			vj_index = iftMGetVoxelIndex(sicle->mimg, vj_voxel);
  			vj_root = (*data)->root_map[vj_index];

  			if((*data)->cost_map[vj_index] != IFTSICLE_BKGCOST) // If not in bkg
  			{
  				// If belongs to the subtree being removed at moment
  				if((*data)->pred_map[vj_index] == vi_index)
  				{
  					(*data)->pred_map[vj_index] = IFTSICLE_NIL; //Temporary predecessor
				    (*data)->root_map[vj_index] = IFTSICLE_NIL; // Temporary root
				    (*data)->cost_map[vj_index] = IFTSICLE_TMPCOST; // Temporary cost
				    iftInsertSet(&remove, vj_index); // Add for BFS removal
  				}
  				else if(vj_root != IFTSICLE_NIL && // If it wasnt removed
  							 	(*data)->root_map[vj_root] != IFTSICLE_NIL && // If relevant
  							 	!iftBMapValue(marked, vj_index)) //If it wasnt yet visited
					{ 
						iftBMapSet1(marked, vj_index); // Visited
						iftInsertSet(&frontier, vj_index); // Frontier of removed tree
					}
  			}
  		}
  	}
  }
  iftDestroyBMap(&marked);

  return frontier;
}

/*
 * Removes the subtree of the given vertex and updates those spels at the 
 * frontier for a new competition in the differential computation.
 *
 * PARAMETERS:
 *  sicle[in] - REQUIRED: SICLE auxiliary data
 *  v_index[in] - REQUIRED: "Root" of the subtree to be removed
 *  data[in/out] - REQUIRED: IFT auxiliary data to be modified
 *  heap[in/out] - REQUIRED: Priority queue of the current IFT execution to
 *													 be updated.
 */
void iftSICLE_RemoveSubtree
(iftSICLE *sicle, int v_index, iftSICLE_IFTData **data, iftDHeap **heap)
{
	iftBMap *marked;
  iftSet *remove, *frontier;

  marked = iftCreateBMap((*data)->num_vtx);

  remove = frontier = NULL;
  iftInsertSet(&remove, v_index);
  while(remove != NULL)
  {
  	int vi_index;
  	iftVoxel vi_voxel;

  	vi_index = iftRemoveSet(&remove);
  	vi_voxel = iftMGetVoxelCoord(sicle->mimg,vi_index);

    (*data)->root_map[vi_index] = IFTSICLE_NIL; // Temporary root
    (*data)->pred_map[vi_index] = IFTSICLE_NIL; // Temporary predecessor
    (*data)->cost_map[vi_index] = IFTSICLE_TMPCOST; // Temporary cost

    if((*heap)->color[vi_index] == IFT_GRAY)
    { iftRemoveDHeapElem((*heap), vi_index); } // Remove if exists in queue
  	else { (*heap)->color[vi_index] = IFT_WHITE; } // Clear its status

  	for(int j = 1; j < (*data)->A->n; ++j)
  	{
  		iftVoxel vj_voxel;

  		vj_voxel = iftGetAdjacentVoxel((*data)->A, vi_voxel, j);

  		if(iftMValidVoxel(sicle->mimg, vj_voxel) == true)
  		{
  			int vj_index;

  			vj_index = iftMGetVoxelIndex(sicle->mimg, vj_voxel);
  			
	  		if((*data)->pred_map[vj_index] == vi_index) // If belongs to subtree
				{ iftInsertSet(&remove, vj_index); } // Add to BFS removal
				else if((*data)->cost_map[vj_index] != IFTSICLE_BKGCOST && // Not bkg
								(*data)->cost_map[vj_index] != IFTSICLE_TMPCOST && // Reached
								 !iftBMapValue(marked, vj_index)) // Visited
				{ 
					iftBMapSet1(marked, vj_index); // Visited
					iftInsertSet(&frontier, vj_index); // Probable frontier
				}
  		}
  	}
  }
	iftDestroyBMap(&marked);

  while(frontier != NULL)
  {
  	int vi_index;

  	vi_index = iftRemoveSet(&frontier);

  	if((*heap)->color[vi_index] == IFT_GRAY) // Already in heap?
		{ iftRemoveDHeapElem((*heap), vi_index); } // Remove for update
		iftInsertDHeap((*heap), vi_index); // Add/update
  }
}

//============================================================================|
// iftSICLE_TStats
//============================================================================|
/*
 * Creates and allocates memory for an Tree statistics instance
 *
 * PARAMETERS:
 *  sicle[in] - REQUIRED: SICLE auxiliary data
 *  args[in] - REQUIRED: SICLE arguments
 *  data[in] - REQUIRED: IFT auxiliary data
 *
 * RETURNS: Instance of the object
 */
iftSICLE_TStats *iftSICLE_CreateTStats
(iftSICLE *sicle, iftSICLEArgs *args, iftSICLE_IFTData *data)
{
	iftSICLE_TStats *tstats;

  tstats = malloc(sizeof(iftSICLE_TStats));
  assert(tstats != NULL);

  tstats->num_trees = data->seeds->n;
  tstats->num_feats = sicle->mimg->m;
  tstats->num_dims = 3;

  tstats->size = calloc(data->seeds->n, sizeof(int));
  assert(tstats->size != NULL);

  tstats->adj = calloc(data->seeds->n, sizeof(iftBMap*));
  assert(tstats->adj != NULL);

  tstats->feats = calloc(data->seeds->n, sizeof(float*));
  assert(tstats->feats != NULL);

  tstats->centr = calloc(data->seeds->n, sizeof(float*));
  assert(tstats->centr != NULL);

  if(sicle->sal == NULL) { tstats->sal = NULL; }
  else
  {
  	tstats->sal = calloc(data->seeds->n, sizeof(float));
	  assert(tstats->sal != NULL);
  }

  for(long s_id = 0; s_id < data->seeds->n; ++s_id)
  { 
	  tstats->adj[s_id] = iftCreateBMap(tstats->num_trees);

    tstats->feats[s_id] = calloc(tstats->num_feats, sizeof(float));
    assert(tstats->feats[s_id] != NULL);

    tstats->centr[s_id] = calloc(tstats->num_dims, sizeof(float));
    assert(tstats->centr[s_id] != NULL);
  }

  return tstats;
}

/*
 * Deallocates the respective object 
 *
 * PARAMETERS:
 *		args[in/out] - REQUIRED: Pointer to the object to be free'd
 */
void iftSICLE_DestroyTStats
(iftSICLE_TStats **tstats)
{
  free((*tstats)->size);
  if((*tstats)->sal != NULL) { free((*tstats)->sal); }
  for(int i = 0; i < (*tstats)->num_trees; ++i)
  {
    iftDestroyBMap(&((*tstats)->adj[i]));
    free((*tstats)->feats[i]);
    free((*tstats)->centr[i]);
  }
  free((*tstats)->adj);
  free((*tstats)->feats);
  free((*tstats)->centr);

  free(*tstats);
  (*tstats) = NULL;
}

/*
 * Calculates the tree statistics of the root map and seed array of the 
 * current IFT execution
 *
 * PARAMETERS:
 *  sicle[in] - REQUIRED: SICLE auxiliary data
 *  args[in] - REQUIRED: SICLE arguments
 *  data[in] - REQUIRED: IFT auxiliary data
 *
 * RETURNS: Extracted tree statistics of the current IFT execution
 */
iftSICLE_TStats *iftSICLE_CalcTStats
(iftSICLE *sicle, iftSICLEArgs *args, iftSICLE_IFTData *data)
{
  iftSICLE_TStats *tstats;

  tstats = iftSICLE_CreateTStats(sicle, args, data);

  for(int vi_index = 0; vi_index < sicle->mimg->n; ++vi_index)
  {
    if(data->cost_map[vi_index] != IFTSICLE_BKGCOST)
    {
    	int vi_label, vi_root;
    	iftVoxel vi_voxel;

    	vi_label = iftSICLE_GetRootLabel(data,vi_index);
    	vi_root = data->root_map[vi_index];
	  	vi_voxel = iftMGetVoxelCoord(sicle->mimg,vi_index);
      
      tstats->size[vi_label]++;
      if(sicle->sal != NULL) // Has saliency?
      { tstats->sal[vi_label] += sicle->sal[vi_index]; }

    	tstats->centr[vi_label][0] += vi_voxel.x;
    	tstats->centr[vi_label][1] += vi_voxel.y;
    	tstats->centr[vi_label][2] += vi_voxel.z;

      for(int f = 0; f < tstats->num_feats; ++f) 
      { tstats->feats[vi_label][f] += sicle->mimg->val[vi_index][f]; }

    	for(int j = 1; j < data->A->n; ++j)
    	{
    		iftVoxel vj_voxel;

    		vj_voxel = iftGetAdjacentVoxel(data->A, vi_voxel, j);

    		if(iftMValidVoxel(sicle->mimg, vj_voxel))
    		{
    			int vj_index, vj_root, vj_label;

    			vj_index = iftMGetVoxelIndex(sicle->mimg, vj_voxel);
    			vj_label = iftSICLE_GetRootLabel(data, vj_index);
    			vj_root = data->root_map[vj_index];

    			// If it is not on the bkg and has different label
    			if(data->cost_map[vj_index] != IFTSICLE_BKGCOST && vi_root!=vj_root)
    			{ iftBMapSet1(tstats->adj[vi_label], vj_label);}
    		}
    	}
    }
  }

  #ifdef IFT_OMP //-----------------------------------------------------------|
	#pragma omp parallel for
	#endif //-------------------------------------------------------------------|
  for(int t_index = 0; t_index < tstats->num_trees; ++t_index) // Compute avg
  {
  	for(int d = 0; d < tstats->num_dims; ++d) 
    { tstats->centr[t_index][d] /= (float)tstats->size[t_index]; }

    for(int j = 0; j < tstats->num_feats; ++j) 
    { tstats->feats[t_index][j] /= (float)tstats->size[t_index]; }

    if(sicle->sal != NULL) // Has saliency?
    { tstats->sal[t_index] /= (float)tstats->size[t_index]; }
  }

  return tstats;
}

//============================================================================|
// Image Foresting Transform
//============================================================================|
/*
 * Computes the connectivity cost between two vertices vi and vj
 *
 * PARAMETERS:
 *  sicle[in] - REQUIRED: SICLE auxiliary data
 *  args[in] - REQUIRED: SICLE arguments
 *  data[in] - REQUIRED: IFT auxiliary data
 *  vi_index[in] - REQUIRED: Path's terminus
 *  vj_index[in] - REQUIRED: Vertex to be conquered
 *
 * RETURNS: Connectivity cost between vi and vj
 */
float iftSICLE_ConnFunction
(iftSICLE *sicle, iftSICLEArgs *args, iftSICLE_IFTData *data, int vi_index, 
	int vj_index)
{
	int vi_root;
	iftVoxel vi_voxel, vj_voxel;
	float *vi_root_feats, *vj_feats;
	double root_feat_dist, spat_dist, sal_dist, arccost, pathcost;

	vi_voxel = iftMGetVoxelCoord(sicle->mimg,vi_index);
	vi_root = data->root_map[vi_index]; 
	vi_root_feats = sicle->mimg->val[vi_root];
	
	vj_voxel = iftMGetVoxelCoord(sicle->mimg,vj_index);
	vj_feats = sicle->mimg->val[vj_index];
	
	root_feat_dist = iftEuclDistance(vi_root_feats, vj_feats, sicle->mimg->m);
	spat_dist = iftVoxelDistance(vi_voxel, vj_voxel);
	
	if(sicle->sal != NULL) // Has saliency?
	{ sal_dist = fabs(sicle->sal[vi_root] - sicle->sal[vj_index]); }
	else { sal_dist = 0.0; }

	pathcost = data->cost_map[vi_index];
	if(args->connopt == IFT_SICLE_CONN_FMAX) // fmax + wroot
	{ 
		arccost = pow(root_feat_dist, 1.0 + args->alpha*sal_dist);
		pathcost = iftMax(pathcost, arccost);
	}
	else if(args->connopt == IFT_SICLE_CONN_FSUM) // fsum + wsum
	{ 
		arccost = (args->irreg + args->alpha*sal_dist) * root_feat_dist;
		arccost = iftFastNatPow(arccost, args->adhr) + spat_dist;
		pathcost += arccost;
	}
	else if(args->connopt == IFT_SICLE_CONN_CUSTOM)
	{
		/* 
			You may write here your own connectivity function for testing possible
			new functionalities for SICLE. For using that, add "--conn-opt custom"
			in the command-line
		*/
		pathcost += 1;
	}
	else
	{ iftError("Unknown connectivity function", __func__); }	

	return pathcost;
}

/*
 * Executes one sequential IFT with the seeds defined in the IFT data provided,
 * which is modified and updated in-place
 *
 * PARAMETERS:
 *  sicle[in] - REQUIRED: SICLE auxiliary data
 *  args[in] - REQUIRED: SICLE arguments
 *  data[in/out] - REQUIRED: IFT auxiliary data to be modified
 */
void iftSICLE_RunSeedIFT
(iftSICLE *sicle, iftSICLEArgs *args, iftSICLE_IFTData **data)
{
	iftDHeap *heap;

	heap = iftCreateDHeap(sicle->mimg->n, (*data)->cost_map);
	iftSetRemovalPolicyDHeap(heap, MINVALUE);

	iftSICLE_ResetIFTData(sicle, data); // Prepare data for the sequential IFT
	for(int s_id = 0; s_id < (*data)->seeds->n; ++s_id) 
	{ iftInsertDHeap(heap, (*data)->seeds->val[s_id]); } // Add seeds

	while(!iftEmptyDHeap(heap))
	{
		int vi_index, vi_root;
		iftVoxel vi_voxel;

		vi_index = iftRemoveDHeap(heap);
		vi_voxel = iftMGetVoxelCoord(sicle->mimg,vi_index);
		vi_root = (*data)->root_map[vi_index]; 

		for(int j = 1; j < (*data)->A->n; ++j)
  	{
  		iftVoxel vj_voxel;

  		vj_voxel = iftGetAdjacentVoxel((*data)->A, vi_voxel, j);

  		if(iftMValidVoxel(sicle->mimg, vj_voxel))
  		{
  			int vj_index;

  			vj_index = iftMGetVoxelIndex(sicle->mimg, vj_voxel);
  			if(heap->color[vj_index] != IFT_BLACK) // Out of the heap?
  			{
  				double pathcost;

  				pathcost = iftSICLE_ConnFunction(sicle, args, *data, vi_index, vj_index);

					if(pathcost < (*data)->cost_map[vj_index]) // Lesser path-cost?
					{
						if(heap->color[vj_index] == IFT_GRAY) // Already within the heap?
						{ iftRemoveDHeapElem(heap, vj_index); } // Remove for update

						(*data)->root_map[vj_index] = vi_root; // 
						(*data)->pred_map[vj_index] = vi_index;// Mark as conquered
						(*data)->cost_map[vj_index] = pathcost;//
						iftInsertDHeap(heap, vj_index);
					}
  			}
  		}
  	}
	}
	iftDestroyDHeap(&heap);
}

/*
 * Executes one differential IFT with the seeds defined in the IFT data 
 * provided, which is modified and updated in-place
 *
 * PARAMETERS:
 *  sicle[in] - REQUIRED: SICLE auxiliary data
 *  args[in] - REQUIRED: SICLE arguments
 *  data[in/out] - REQUIRED: IFT auxiliary data to be modified
 *  irre_seeds[in/out] - REQUIRED: Irrelevant seeds to be removed
 */
void iftSICLE_RunSeedDIFT
(iftSICLE *sicle, iftSICLEArgs *args, iftSICLE_IFTData **data, 
	iftSet **irre_seeds)
{
	iftSet *frontier;
	iftDHeap *heap;

	frontier = iftSICLE_RemoveTrees(sicle, data, irre_seeds);//Remove irrelevants
	heap = iftCreateDHeap(sicle->mimg->n, (*data)->cost_map);
	iftSetRemovalPolicyDHeap(heap, MINVALUE);

	#ifdef IFT_OMP //-----------------------------------------------------------|
	#pragma omp parallel for
	#endif //-------------------------------------------------------------------|
	for(int s_id = 0; s_id < (*data)->seeds->n; ++s_id) // Update labels
	{ (*data)->pred_map[(*data)->seeds->val[s_id]] = -(s_id + 1); } // 2's compl

	while(frontier != NULL)
	{ iftInsertDHeap(heap, iftRemoveSet(&frontier)); }

	while(iftEmptyDHeap(heap) == false)
	{
		int vi_index, vi_root;
		iftVoxel vi_voxel;

		vi_index = iftRemoveDHeap(heap);
		vi_voxel = iftMGetVoxelCoord(sicle->mimg,vi_index);
		vi_root = (*data)->root_map[vi_index]; 

		for(int j = 1; j < (*data)->A->n; ++j)
  	{
  		iftVoxel vj_voxel;

  		vj_voxel = iftGetAdjacentVoxel((*data)->A, vi_voxel, j);

  		if(iftMValidVoxel(sicle->mimg, vj_voxel))
  		{
  			int vj_index;

  			vj_index = iftMGetVoxelIndex(sicle->mimg, vj_voxel);
  			if(heap->color[vj_index] != IFT_BLACK) // Out of the heap?
  			{
  				double pathcost;
		
  				pathcost = iftSICLE_ConnFunction(sicle, args, *data, vi_index, vj_index);

					if(pathcost < (*data)->cost_map[vj_index]) // Lesser path-cost?
					{
						if(heap->color[vj_index] == IFT_GRAY) // Already within the heap?
						{ iftRemoveDHeapElem(heap, vj_index); } // Remove for update

						(*data)->root_map[vj_index] = vi_root; //
						(*data)->pred_map[vj_index] = vi_index;// Mark as conquered
						(*data)->cost_map[vj_index] = pathcost;//
						iftInsertDHeap(heap, vj_index);
					}
					else if(vi_index == (*data)->pred_map[vj_index])
					{
						if(pathcost > (*data)->cost_map[vj_index] || 
							 vi_root != (*data)->root_map[vj_index])
							//Inconsistency -> Remove and Compete again
						{ iftSICLE_RemoveSubtree(sicle, vj_index, data, &heap);}
					}
  			}
  		}
  	}
	}
	iftDestroyDHeap(&heap);
}

//============================================================================|
// Seed Removal
//============================================================================|
/*
 * Calculate the priority/relevance of the seeds based on the predefined 
 * criterion and penalization. 
 *
 * PARAMETERS:
 *  sicle[in] - REQUIRED: SICLE auxiliary data
 *  args[in] - REQUIRED: SICLE arguments
 *  data[in] - REQUIRED: IFT auxiliary data
 *
 * RETURNS: Double |S|-sized array of seeds' priority/relevance
 */
double *iftSICLE_CalcSeedPrio
(iftSICLE *sicle, iftSICLEArgs *args, iftSICLE_IFTData *data)
{
  double *prio;
  iftSICLE_TStats *tstats;

  prio = calloc(data->seeds->n, sizeof(double));
  tstats = iftSICLE_CalcTStats(sicle, args, data);
	
  #ifdef IFT_OMP //-----------------------------------------------------------|
	#pragma omp parallel for
	#endif //-------------------------------------------------------------------|
  for(long ti_index = 0; ti_index < tstats->num_trees; ++ti_index)
  {
    double size_perc, min_color_grad, max_sal_grad, min_dist, num_adjs,
    			 max_color_grad, dist_perc;

    size_perc = tstats->size[ti_index]/(float)sicle->mimg->n; 

    max_sal_grad = max_color_grad = 0.0;
    min_color_grad = min_dist = IFT_INFINITY_DBL;
    num_adjs = 0;
  	for(long tj_index = 0; tj_index < tstats->num_trees; ++tj_index)
    {
    	if(iftBMapValue(tstats->adj[ti_index],tj_index))
    	{
    		double sal_grad, grad, dist;
	     	
	     	num_adjs++; 
	      grad= iftEuclDistance(tstats->feats[ti_index], tstats->feats[tj_index],
	                             tstats->num_feats);
	      dist= iftEuclDistance(tstats->centr[ti_index], tstats->centr[tj_index],
	      											 tstats->num_dims);

	      if(grad < min_color_grad) { min_color_grad = grad; }
	      if(grad > max_color_grad) { max_color_grad = grad; }
	      if(dist < min_dist) { min_dist = dist; }

	      if(sicle->sal != NULL) // Has saliency?
	      { 
	      	sal_grad = fabs(tstats->sal[ti_index] - tstats->sal[tj_index]); 
	      	if(sal_grad > max_sal_grad) { max_sal_grad = sal_grad; }
	      }
    	}
    }
	dist_perc = min_dist/iftDiagonalSize(sicle->mimg);

		if(args->critopt == IFT_SICLE_CRIT_SIZE)
    { prio[ti_index] = size_perc; }
    else if(args->critopt == IFT_SICLE_CRIT_MINSC)
    { prio[ti_index] = size_perc * min_color_grad; }
    else if(args->critopt == IFT_SICLE_CRIT_MAXSC)
    { prio[ti_index] = size_perc * max_color_grad; }
  	else if(args->critopt == IFT_SICLE_CRIT_SPREAD)
    { prio[ti_index] = size_perc * min_dist; }
		else if(args->critopt == IFT_SICLE_CRIT_CUSTOM)
    {
    	/* 
				You may write here your own criterion function for testing possible
				new functionalities for SICLE. For using that, add "--crit-opt custom"
				in the command-line
			*/
    	prio[ti_index] = size_perc*1.0/min_dist;
    }
    else 
    { iftError("Unknown seed removal criterion fuction", __func__); }

    if(args->penopt == IFT_SICLE_PEN_OBJ)
  	{ prio[ti_index]*= iftMax(tstats->sal[ti_index], max_sal_grad); }
  	else if(args->penopt == IFT_SICLE_PEN_BORD)
  	{ prio[ti_index]*= max_sal_grad; }
  	else if(args->penopt == IFT_SICLE_PEN_OSB)
  	{
  	  float bkg_relevance;

  	  bkg_relevance = (1.0 - tstats->sal[ti_index])*dist_perc;
  	  prio[ti_index]*= iftMax(tstats->sal[ti_index],bkg_relevance);
  	}
  	else if(args->penopt == IFT_SICLE_PEN_BOBS)
  	{
  	  float bkg_relevance, obj_relevance;
			
			obj_relevance = tstats->sal[ti_index]*max_sal_grad;
  	  bkg_relevance = (1.0 - tstats->sal[ti_index])*dist_perc;
  	  prio[ti_index]*= iftMax(obj_relevance,bkg_relevance);
  	}
  	else if(args->penopt == IFT_SICLE_PEN_CUSTOM)
  	{
    	/* 
				You may write here your own penalization function for testing possible
				new functionalities for SICLE. For using that, add "--pen-opt custom"
				in the command-line
			*/
  	  prio[ti_index]*= 1;
  	}
  	else if(args->penopt != IFT_SICLE_PEN_NONE)
		{ iftError("Unknown seed relevance penalization", __func__); }
  }
  iftSICLE_DestroyTStats(&tstats);

  return prio;
}

/*
 * Removes Ni irrelevant seeds for the next IFT execution
 *
 * PARAMETERS:
 *  sicle[in] - REQUIRED: SICLE auxiliary data
 *  num_maint[in] - REQUIRED: Number of seeds to be maintained
 *  args[in] - REQUIRED: SICLE arguments
 *  data[in/out] - REQUIRED: IFT auxiliary data to be modified
 *  irre_seeds[in/out] - REQUIRED: Irrelevant seeds selected to be removed
 */
void iftSICLE_RemSeeds
(iftSICLE *sicle, int num_maint, iftSICLEArgs *args, iftSICLE_IFTData **data,
 iftSet **irre_seeds)
{
  double *prio;
  iftIntArray *new_seeds;
  iftDHeap *heap;

  prio = iftSICLE_CalcSeedPrio(sicle, args, (*data));
  heap = iftCreateDHeap((*data)->seeds->n, prio);
  iftSetRemovalPolicyDHeap(heap, MAXVALUE);

  new_seeds = iftCreateIntArray(num_maint); // Empty array

  for(long s_id = 0; s_id < (*data)->seeds->n; ++s_id) // Add all for ordering
  { iftInsertDHeap(heap, s_id); } 

  for(long i = 0; i < num_maint; ++i) // Add relevants
  { new_seeds->val[i] = (*data)->seeds->val[iftRemoveDHeap(heap)]; }

	(*irre_seeds) = NULL;
  while(!iftEmptyDHeap(heap)) // Add irrelevants for removal
  { iftInsertSet(irre_seeds, (*data)->seeds->val[iftRemoveDHeap(heap)]); }
	iftDestroyIntArray(&((*data)->seeds));
	(*data)->seeds = new_seeds;

  free(prio);
  iftDestroyDHeap(&heap);
}

//############################################################################|
// 
//	PUBLIC METHODS
//
//############################################################################|
//============================================================================|
// iftSICLEArgs
//============================================================================|
iftSICLEArgs *iftCreateSICLEArgs()
{
	iftSICLEArgs *args;

	args = malloc(sizeof(iftSICLEArgs));
	assert(args != NULL);

	args->use_diag = true;
	args->use_dift = true;
	args->n0 = 3000;
	args->nf = 200;
	args->max_iters = 5;
	args->irreg = 0.12;
	args->adhr = 12;
	args->alpha = 0.0;
	args->user_ni = NULL;
	args->samplopt = IFT_SICLE_SAMPL_RND;
	args->connopt = IFT_SICLE_CONN_FMAX;
	args->critopt = IFT_SICLE_CRIT_MINSC;
	args->penopt = IFT_SICLE_PEN_NONE;

	return args;
}

inline void iftDestroySICLEArgs
(iftSICLEArgs **args)
{
	#ifdef IFT_DEBUG //---------------------------------------------------------|
	assert(args != NULL);
	#endif //-------------------------------------------------------------------|
	if((*args) != NULL) 
	{
		if((*args)->user_ni != NULL) { iftDestroyIntArray(&((*args)->user_ni)); } 
		free(*args); 
		(*args) = NULL; 
	}
}

//============================================================================|
// iftSICLE
//============================================================================|
iftSICLE *iftCreateSICLE
(iftImage *img, iftImage *objsm, iftImage *mask)
{
	#ifdef IFT_DEBUG //---------------------------------------------------------|
	assert(img != NULL);
	if(objsm != NULL) { iftVerifyImageDomains(img, objsm, __func__); }
	if(mask != NULL) { iftVerifyImageDomains(img, mask, __func__); }
	#endif //-------------------------------------------------------------------|
	iftSICLE *sicle;

	sicle = malloc(sizeof(iftSICLE));
	assert(sicle != NULL);

	if(iftIsColorImage(img)) 
	{ sicle->mimg = iftImageToMImage(img, LAB_CSPACE); }
	else { sicle->mimg = iftImageToMImage(img, GRAY_CSPACE); }
	
	if(mask != NULL) { sicle->roi = iftBinImageToBMap(mask); }
	else { sicle->roi = NULL; }

	if(objsm != NULL)
	{
		int max_sal;

		sicle->sal = calloc(sicle->mimg->n, sizeof(float));
		assert(sicle->sal);

		max_sal = 0;
		#ifdef IFT_OMP //---------------------------------------------------------|
		#pragma omp parallel for reduction(+:max_sal)
		#endif //-----------------------------------------------------------------|
		for(int v_index = 0; v_index < sicle->mimg->n; ++v_index)
		{ max_sal = iftMax(max_sal, objsm->val[v_index]); }

		#ifdef IFT_OMP //---------------------------------------------------------|
		#pragma omp parallel for
		#endif //-----------------------------------------------------------------|
		for(int v_index = 0; v_index < sicle->mimg->n; ++v_index)
		{ sicle->sal[v_index] = (float)objsm->val[v_index]/max_sal; } // Norm [0,1]
	}
	else { sicle->sal = NULL; }

	return sicle;
}

void iftDestroySICLE
(iftSICLE **sicle)
{
	#ifdef IFT_DEBUG //---------------------------------------------------------|
	assert(sicle != NULL);
	#endif //-------------------------------------------------------------------|
	if((*sicle) != NULL)
	{
		iftDestroyMImage(&((*sicle)->mimg));
		if((*sicle)->sal != NULL) { free((*sicle)->sal); }
		if((*sicle)->roi != NULL) iftDestroyBMap(&((*sicle)->roi));
		free(*sicle);
		(*sicle) = NULL;
	}
}

//============================================================================|
// Runner
//============================================================================|
void iftVerifySICLEArgs
(iftSICLE *sicle, iftSICLEArgs *args)
{
	#ifdef IFT_DEBUG //---------------------------------------------------------|
	assert(sicle != NULL); assert(args != NULL);
	#endif //-------------------------------------------------------------------|
	int num_vtx;

	if(sicle->roi == NULL) { num_vtx = sicle->mimg->n; }
	else
	{
		num_vtx = 0;
		#ifdef IFT_OMP //---------------------------------------------------------|
		#pragma omp parallel for
		#endif //-----------------------------------------------------------------|
		for(int v_index = 0; v_index < sicle->mimg->n; ++v_index)
		{ if(iftBMapValue(sicle->roi, v_index) == true) { num_vtx++; } }
	}

	if(args->n0 >= num_vtx || args->n0 <= 2)
	{ 
		iftError("Invalid N0 value of %d. It must be within ]2,%d[\n", __func__,
							args->n0, num_vtx); 
	}

	if(args->nf >= args->n0)
	{ 
		iftError("Invalid Nf value of %d. It must be within [2,%d[\n", __func__,
							args->nf, args->n0); 
	}

	if(args->max_iters < 2)
	{
		iftError("Invalid quantity of %d iterations. It must be >= 2\n", __func__,
							args->max_iters); 	
	}

	if(args->irreg < 0.0)
	{
		iftError("Invalid irregularity value of %f. It must be >= 0\n", __func__,
							args->irreg); 	
	}

	if(args->adhr < 0)
	{
		iftError("Invalid boundary adherence value of %f. It must be >= 0\n", 
							__func__, args->adhr); 	
	}

	if(args->alpha < 0.0)
	{
		iftError("Invalid boosting factor of %f. It must be within [0,1]\n", 
							__func__, args->alpha); 	
	}

	if(args->user_ni != NULL)
	{
		if(args->user_ni->val[0] >= args->n0 || 
			args->user_ni->val[args->user_ni->n - 1] <= args->nf)
		{
			iftError("intermediary values must be within ]N0,...,Ni,...,Nf[\n",
							__func__);	
		}
		for(long i = 1; i < args->user_ni->n; ++i)
		{
			if(args->user_ni->val[i-1] <= args->user_ni->val[i])
			{
				iftError("Ni values must be strictly decreasing\n",__func__); 	
			}
		}
	}
	if(sicle->sal == NULL && args->penopt != IFT_SICLE_PEN_NONE)
	{
		iftError("Penalization cannot be used without a saliency map\n",
							__func__); 	
	}
			
}

iftImage *iftRunSICLE
(iftSICLE *sicle, iftSICLEArgs *args)
{
	#ifdef IFT_DEBUG //---------------------------------------------------------|
	assert(sicle != NULL);
	if(args != NULL) { iftVerifySICLEArgs(sicle,args); }
	#endif //-------------------------------------------------------------------|
	bool default_args;
	iftSICLE_IFTData *data;
	iftSet *irre_seeds;
	iftIntArray *ni;
	iftImage *segm;

	if(args == NULL) { args = iftCreateSICLEArgs(); default_args = true;}
	else { default_args = false; }

	data = iftSICLE_CreateIFTData(sicle, args);
	ni = iftSICLE_CreateNiArray(args, data);

	irre_seeds = NULL;
  for(int it = 1; it < ni->n; ++it)
  {
  	#ifdef IFT_DEBUG //-------------------------------------------------------|
		fprintf(stderr, "DEBUG (%s): Iteration %d\n", __func__, it);
		#endif //-----------------------------------------------------------------|	
		if(args->use_dift == false || it == 1) // Seq or 1st iter?
		{ iftSICLE_RunSeedIFT(sicle, args, &data); }
		else 
		{ iftSICLE_RunSeedDIFT(sicle, args, &data, &irre_seeds); }

		#ifdef IFT_DEBUG //-------------------------------------------------------|
		iftImage *seed_img = iftSICLE_CreateSeedImage(sicle, data);
		iftWriteImageByExt(seed_img, "debug/seeds_%d_%d.pgm", it, ni->val[it-1]);
		iftDestroyImage(&seed_img);
		iftImage *segm_img = iftSICLE_CreateLabelImage(sicle, data);
		iftWriteImageByExt(segm_img, "debug/segm_%d_%d.pgm", it, ni->val[it-1]);
		iftDestroyImage(&segm_img);
		fprintf(stderr, "DEBUG (%s): Ni = %d\n", __func__, ni->val[it]);
		#endif //-----------------------------------------------------------------|		
		
		iftSICLE_RemSeeds(sicle, ni->val[it], args,&data, &irre_seeds);
  }
	#ifdef IFT_DEBUG //---------------------------------------------------------|
	fprintf(stderr, "DEBUG (%s): Last iteration\n", __func__);
	#endif //-------------------------------------------------------------------|	
	// Segmentation with Nf seeds
	if(args->use_dift == false) // Seq?
	{ iftSICLE_RunSeedIFT(sicle, args, &data); }
	else 
	{ iftSICLE_RunSeedDIFT(sicle, args, &data, &irre_seeds); }
	
  #ifdef IFT_DEBUG //---------------------------------------------------------|
	iftImage *seed_img = iftSICLE_CreateSeedImage(sicle, data);
	iftWriteImageByExt(seed_img, "debug/seeds_%d_%d.pgm", ni->n, args->nf);
	iftDestroyImage(&seed_img);
	iftImage *segm_img = iftSICLE_CreateLabelImage(sicle, data);
	iftWriteImageByExt(segm_img, "debug/segm_%d_%d.pgm", ni->n, args->nf);
	iftDestroyImage(&segm_img);
	#endif //-------------------------------------------------------------------|

	segm = iftSICLE_CreateLabelImage(sicle, data);

	if(default_args) { iftDestroySICLEArgs(&args); }
	iftSICLE_DestroyIFTData(&data);
	iftDestroyIntArray(&ni);
	return segm;
}

iftImage **iftRunMultiscaleSICLE
(iftSICLE *sicle, iftSICLEArgs *args, int *num_scales)
{
	#ifdef IFT_DEBUG //---------------------------------------------------------|
	assert(sicle != NULL);
	if(args != NULL) { iftVerifySICLEArgs(sicle,args); }
	#endif //-------------------------------------------------------------------|
	bool default_args;
	iftSICLE_IFTData *data;
	iftSet *irre_seeds;
	iftIntArray *ni;
	iftImage **segm;

	if(args == NULL) { args = iftCreateSICLEArgs(); default_args = true;}
	else { default_args = false; }

	data = iftSICLE_CreateIFTData(sicle, args);
	ni = iftSICLE_CreateNiArray(args, data);
	(*num_scales) = ni->n;
	segm = calloc(ni->n, sizeof(iftImage*));

	irre_seeds = NULL;
  for(int it = 1; it < ni->n; ++it)
  {
  	#ifdef IFT_DEBUG //-------------------------------------------------------|
		fprintf(stderr, "DEBUG (%s): Iteration %d\n", __func__, it);
		#endif //-----------------------------------------------------------------|	
		if(args->use_dift == false || it == 1) // Seq or 1st iter?
		{ iftSICLE_RunSeedIFT(sicle, args, &data); }
		else 
		{ iftSICLE_RunSeedDIFT(sicle, args, &data, &irre_seeds); }
		segm[it - 1] = iftSICLE_CreateLabelImage(sicle, data);

		#ifdef IFT_DEBUG //-------------------------------------------------------|
		iftImage *seed_img = iftSICLE_CreateSeedImage(sicle, data);
		iftWriteImageByExt(seed_img, "debug/seeds_%d_%d.pgm", it, ni->val[it-1]);
		iftDestroyImage(&seed_img);
		fprintf(stderr, "DEBUG (%s): Ni = %d\n", __func__, ni->val[it]);
		#endif //-----------------------------------------------------------------|		
		iftSICLE_RemSeeds(sicle, ni->val[it], args,&data, &irre_seeds);
  }

	// Segmentation with Nf seeds
	#ifdef IFT_DEBUG //---------------------------------------------------------|
	fprintf(stderr, "DEBUG (%s): Last iteration\n", __func__);
	#endif //-------------------------------------------------------------------|	
	if(args->use_dift == false) // Seq?
	{ iftSICLE_RunSeedIFT(sicle, args, &data); }
	else 
	{ iftSICLE_RunSeedDIFT(sicle, args, &data, &irre_seeds); }
	segm[ni->n - 1] = iftSICLE_CreateLabelImage(sicle, data);

  #ifdef IFT_DEBUG //---------------------------------------------------------|
	iftImage *seed_img = iftSICLE_CreateSeedImage(sicle, data);
	iftWriteImageByExt(seed_img, "debug/seeds_%d_%d.pgm", ni->n, args->nf);
	iftDestroyImage(&seed_img);
	#endif //-------------------------------------------------------------------|

	if(default_args) { iftDestroySICLEArgs(&args); }
	iftSICLE_DestroyIFTData(&data);
	iftDestroyIntArray(&ni);

	return segm;
}

