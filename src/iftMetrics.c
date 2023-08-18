/*****************************************************************************\
* iftMetrics.c
*
* AUTHOR  : Felipe Belem
* DATE    : 2021-03-08
* LICENSE : MIT License
* EMAIL   : felipe.belem@ic.unicamp.br
\*****************************************************************************/
#include "iftMetrics.h"

//############################################################################|
// 
//  PRIVATE METHODS
//
//############################################################################|
//===========================================================================//
// General & Auxiliary
//===========================================================================//
/*
 * Gets the size of the intersections between ground-truth objects and 
 * superspels. 
 *
 * PARAMETERS:
 *  label_img[in] - REQUIRED: Superspel segmentation
 *  gt_img[in] - REQUIRED: Ground-truth
 *
 * RETURNS: Matrix Superspel x GT Object, whose value is the intersection size
 */
int **iftMetrics_CalcLabelGTIntersec
(iftImage *label_img, iftImage *gt_img)
{
  int min_label, max_label, num_labels, min_gt, max_gt, num_gt;
  int **inter;

  iftMinMaxValues(label_img, &min_label, &max_label);
  num_labels = max_label - min_label + 1;
  iftMinMaxValues(gt_img, &min_gt, &max_gt);
  num_gt = max_gt - min_gt + 1;

  inter = calloc(num_labels, sizeof(int*));
  assert(inter != NULL);

  for(int i = 0; i < num_labels; ++i)
  {
    inter[i] = calloc(num_gt, sizeof(int));
    assert(inter[i] != NULL);
  }

  for(int p = 0; p < label_img->n; ++p)
  {
    int label, gt;

    label = label_img->val[p] - min_label;
    gt = gt_img->val[p] - min_gt;

    inter[label][gt]++;
  }

  return inter;
}

//############################################################################|
// 
//  PUBLIC METHODS
//
//############################################################################|
//============================================================================|
// Auxiliary
//============================================================================|
iftImage *iftRelabelImage
(iftImage *label_img)
{
  #ifdef IFT_DEBUG //---------------------------------------------------------|
  assert(label_img != NULL);
  #endif //-------------------------------------------------------------------|
  int new_label;
  iftSet *queue;
  iftImage *relabel_img;
  iftAdjRel *A;
  iftBMap *visited;

  relabel_img = iftCreateImage(label_img->xsize, label_img->ysize, label_img->zsize);
  if(iftIs3DImage(label_img)) { A = iftSpheric(sqrtf(3.0)); }
  else { A = iftCircular(sqrtf(2.0)); }

  new_label = 0;
  queue = NULL;
  visited = iftCreateBMap(label_img->n);

  for(int p = 0; p < label_img->n; ++p)
  {
    if(!iftBMapValue(visited, p))  
    {
      iftInsertSet(&queue, p); ++new_label;
      while(queue != NULL)
      {
        int x;
        iftVoxel x_vxl;

        x = iftRemoveSet(&queue);
        x_vxl = iftGetVoxelCoord(label_img, x);
        iftBMapSet1(visited, x);
        relabel_img->val[x] = new_label;

        for(int i = 1; i < A->n; ++i)
        {
          iftVoxel y_vxl;

          y_vxl = iftGetAdjacentVoxel(A, x_vxl, i);  

          if(iftValidVoxel(label_img, y_vxl))
          {
            int y;

            y = iftGetVoxelIndex(label_img, y_vxl);

            if(label_img->val[x] == label_img->val[y] && !iftBMapValue(visited, y))
            { iftInsertSet(&queue, y); }
          }
        }
      }
    }
  }

  iftDestroySet(&queue);
  iftDestroyBMap(&visited);
  iftDestroyAdjRel(&A);

  return relabel_img;
}

//============================================================================|
// Non-GT-based
//============================================================================|
float iftEvalCO
(iftImage *label_img)
{
  #ifdef IFT_DEBUG //---------------------------------------------------------|
  assert(label_img != NULL);
  #endif //-------------------------------------------------------------------|
  int  min_label, max_label, num_labels;
	float compac;
	int *sup_area, *sup_perim; // Superspels' total and surface size
  iftAdjRel *A;

  iftMinMaxValues(label_img, &min_label, &max_label);
  num_labels = max_label - min_label + 1;

  sup_area = calloc(num_labels, sizeof(int));
  assert(sup_area != NULL);
  sup_perim = calloc(num_labels, sizeof(int));
  assert(sup_perim != NULL);

  if(iftIs3DImage(label_img) == true)
  { A = iftSpheric(sqrtf(3.0)); }
  else
  { A = iftCircular(sqrtf(2.0)); }
	
  for(int p = 0; p < label_img->n; ++p)
  {
    bool is_border;
    int p_label;
    iftVoxel p_vxl;

    p_vxl = iftGetVoxelCoord(label_img, p);
    p_label = label_img->val[p] - min_label;

    is_border = false;
    for(int i = 1; i < A->n && !is_border; ++i)
    {
      iftVoxel adj_vxl;

      adj_vxl = iftGetAdjacentVoxel(A, p_vxl, i);

      if(iftValidVoxel(label_img, adj_vxl) == true) 
      {
        int adj_idx;

        adj_idx = iftGetVoxelIndex(label_img, adj_vxl);
				
				if(label_img->val[p] != label_img->val[adj_idx])
				{ is_border = true; sup_perim[p_label]++; }
      }
			else { is_border = true; sup_perim[p_label]++; } // Limits of the image
    }
		sup_area[p_label]++;
  }
	iftDestroyAdjRel(&A);

	compac = 0.0;
  #ifdef IFT_OMP //-----------------------------------------------------------|
  #pragma omp parallel for reduction(+:compac)
  #endif //-------------------------------------------------------------------|
  for(int i = 0; i < num_labels; ++i)
  {
		float ratio;

    // Normalize to [0,1]
    if(iftIs3DImage(label_img) == false)
		{ ratio = (4.0 * IFT_PI * sup_area[i])/(sup_perim[i]*sup_perim[i]); }
    else
    { ratio = (6.0 * sqrtf(IFT_PI) * sup_area[i])/(pow(sup_perim[i],1.5)); }
		compac += ratio * sup_area[i]/(float)label_img->n;
  }
	free(sup_area);
	free(sup_perim);

  return compac;
}

float iftEvalCD
(iftImage *label_img)
{
  #ifdef IFT_DEBUG //---------------------------------------------------------|
  assert(label_img != NULL);
  #endif //-------------------------------------------------------------------|
  int count_borders;
	float cont_dens;
  iftAdjRel *A;

  if(iftIs3DImage(label_img) == true)
  { A = iftSpheric(sqrtf(3.0)); }
  else
  { A = iftCircular(sqrtf(2.0)); }
	
	count_borders = 0;
  #ifdef IFT_OMP //-----------------------------------------------------------|
  #pragma omp parallel for reduction(+:count_borders)
  #endif //-------------------------------------------------------------------|
  for(int p = 0; p < label_img->n; ++p)
  {
    bool is_border;
    iftVoxel p_vxl;
    
    p_vxl = iftGetVoxelCoord(label_img, p);

    is_border = false;
    for(int i = 1; i < A->n && !is_border; ++i)
    {
      iftVoxel adj_vxl;

      adj_vxl = iftGetAdjacentVoxel(A, p_vxl, i);

      if(iftValidVoxel(label_img, adj_vxl) == true) 
      {
        int adj_idx;

        adj_idx = iftGetVoxelIndex(label_img, adj_vxl);
				
				if(label_img->val[p] != label_img->val[adj_idx])
				{ is_border = true; count_borders++; }
      }
			else { is_border = true; count_borders++; } // Limits of the image
    }
  }
  iftDestroyAdjRel(&A);

  // Normalize to [0,1]
  cont_dens = count_borders/(float)label_img->n;

  return cont_dens;
}

float iftEvalEV
(iftImage *label_img, iftImage *orig_img)
{
  #ifdef IFT_DEBUG //---------------------------------------------------------|
  assert(label_img != NULL);
  assert(orig_img != NULL);
  iftVerifyImageDomains(orig_img, label_img, __func__);
  #endif //-------------------------------------------------------------------|
  int min_label, max_label, num_labels, num_feats;
  float expl_var, img_diff;
  int *sup_size;
  float *mean_img;
  float **mean_sup;

  iftMinMaxValues(label_img, &min_label, &max_label);
  num_labels = max_label - min_label + 1;
  num_feats = (iftIsColorImage(orig_img) == true) ? 3 : 1;

  mean_img = calloc(num_feats, sizeof(float));
  assert(mean_img != NULL);
  sup_size = calloc(num_labels, sizeof(int));
  assert(sup_size != NULL);
  mean_sup = calloc(num_labels, sizeof(float*));
  assert(mean_sup != NULL);

  for(int i = 0; i < num_labels; ++i)
  {
    mean_sup[i] = calloc(num_feats, sizeof(float));
    assert(mean_sup[i] != NULL);  
  }

  for(int p = 0; p < orig_img->n; ++p)
  {
    int p_label;

    p_label = label_img->val[p] - min_label;

    mean_img[0] += orig_img->val[p];         
    mean_sup[p_label][0] += orig_img->val[p];
    if(num_feats > 1)
    {
      mean_img[1] += orig_img->Cb[p];         
      mean_sup[p_label][1] += orig_img->Cb[p];

      mean_img[2] += orig_img->Cr[p];         
      mean_sup[p_label][2] += orig_img->Cr[p];
    }

    sup_size[p_label]++;
  }

  for(int j = 0; j < num_feats; ++j)
  {
    mean_img[j] /= (float)orig_img->n;
    for(int i = 0; i < num_labels; ++i)
      mean_sup[i][j] /= (float)sup_size[i];
  }

  img_diff = 0.0;
  #ifdef IFT_OMP //-----------------------------------------------------------|
  #pragma omp parallel for reduction(+:img_diff)
  #endif //-------------------------------------------------------------------|
  for(int p = 0; p < orig_img->n; ++p)
  {
    float dist;

    dist = orig_img->val[p] - mean_img[0];
    if(num_feats > 1)
    {
      dist += orig_img->Cb[p] - mean_img[1];
      dist += orig_img->Cr[p] - mean_img[2];
    }

    img_diff += (dist * dist); // sum((spel - mean(I))^2)
  }

  expl_var = 0.0;
  #ifdef IFT_OMP //-----------------------------------------------------------|
  #pragma omp parallel for reduction(+:expl_var)
  #endif //-------------------------------------------------------------------|
  for(int i = 0; i < num_labels; ++i)
  {
    float dist;

    dist = 0;
    for(int j = 0; j < num_feats; ++j)
    { dist += (mean_sup[i][j] - mean_img[j]); }
    expl_var += (sup_size[i] * (dist * dist))/img_diff;
  }

  for(int i = 0; i < num_labels; ++i)
  { free(mean_sup[i]); }
  free(mean_sup);
  free(sup_size);
  free(mean_img);

  return expl_var;
}

float iftEvalTEX
(iftImage *label_img)
{
  #ifdef IFT_DEBUG //---------------------------------------------------------|
  assert(label_img != NULL);
  #endif //-------------------------------------------------------------------|
  int  min_label, max_label, num_labels;
  float tex;
  int *min_frame, *max_frame;

  iftMinMaxValues(label_img, &min_label, &max_label);
  num_labels = max_label - min_label + 1;

  min_frame = calloc(num_labels, sizeof(int));
  assert(min_frame != NULL);
  max_frame = calloc(num_labels, sizeof(int));
  assert(max_frame != NULL);
  
  for(int i = 0; i < num_labels; ++i)
  { min_frame[i] = label_img->zsize - 1; max_frame[i] = 0; }

  for(int p_z = 0; p_z < label_img->zsize; ++p_z)
  {
    for(int p_y = 0; p_y < label_img->ysize; ++p_y)
    {
      for(int p_x = 0; p_x < label_img->xsize; ++p_x)
      {
        int p_idx, p_label;
        iftVoxel p_vxl;

        p_vxl.x = p_x; p_vxl.y = p_y; p_vxl.z = p_z;
        p_idx = iftGetVoxelIndex(label_img, p_vxl);
        p_label = label_img->val[p_idx] - min_label;

        min_frame[p_label] = iftMin(min_frame[p_label], p_z);
        max_frame[p_label] = iftMax(min_frame[p_label], p_z);
      }
    }
  }

  tex = 0;
  for(int i = 0; i < num_labels; ++i)
  { tex += max_frame[i] - min_frame[i] + 1;}
  tex /= (float)(label_img->zsize * num_labels);

  free(min_frame);
  free(max_frame);

  return tex;
}

//===========================================================================//
// GT-based
//===========================================================================//
float iftEvalASA
(iftImage *label_img, iftImage *gt_img)
{
  #ifdef IFT_DEBUG //---------------------------------------------------------|
  assert(label_img != NULL);
  assert(gt_img != NULL);
  iftVerifyImageDomains(label_img, gt_img, __func__);
  #endif //-------------------------------------------------------------------|
  int min_label, max_label, num_labels, min_gt, max_gt, num_gt;
  float achi_segm;
  int **inter;

  iftMinMaxValues(label_img, &min_label, &max_label);
  num_labels = max_label - min_label + 1;
  iftMinMaxValues(gt_img, &min_gt, &max_gt);
  num_gt = max_gt - min_gt + 1;

  inter = iftMetrics_CalcLabelGTIntersec(label_img, gt_img);

  achi_segm = 0.0;
  #ifdef IFT_OMP //-----------------------------------------------------------|
  #pragma omp parallel for reduction(+:achi_segm)
  #endif //-------------------------------------------------------------------|
  for(int i = 0; i < num_labels; ++i)
  {
    int max_inter;

    max_inter = 0;
    for(int j = 0; j < num_gt; ++j)
    { 
      if(inter[i][j] > max_inter) { max_inter = inter[i][j]; }
    }
    achi_segm += max_inter;
  }
  for(int i = 0; i < num_labels; ++i) 
  { free(inter[i]); }
  free(inter);

  achi_segm /= (float)label_img->n;

  return achi_segm;
}

float iftEvalBR
(iftImage *label_img, iftImage *gt_img)
{
  #ifdef IFT_DEBUG //---------------------------------------------------------|
  assert(label_img != NULL);
  assert(gt_img != NULL);
  iftVerifyImageDomains(label_img, gt_img, __func__);
  #endif //-------------------------------------------------------------------|
  int tp, fn;
  float bound_rec, r; 
  iftAdjRel *A;

  r = ceil(0.0025 * iftDiagonalSize(label_img)); // Stutz et al.
  if(iftIs3DImage(label_img) == true)
  { A = iftSpheric(r * sqrtf(3.0)); } // = cube whose l = (2r+1)
  else
  { A = iftCircular(r * sqrtf(2.0)); } // = square whose l = (2r+1)

  tp = fn = 0;
  #ifdef IFT_OMP //-----------------------------------------------------------|
  #pragma omp parallel for reduction(+:tp, fn)
  #endif //-------------------------------------------------------------------|
  for(int p = 0; p < label_img->n; ++p)
  {
    bool is_label_border, is_gt_border;
    iftVoxel p_vxl;
    
    p_vxl = iftGetVoxelCoord(label_img, p);

    is_gt_border = is_label_border = false;
    for(int i = 1; i < A->n; ++i)
    {
      iftVoxel adj_vxl;

      adj_vxl = iftGetAdjacentVoxel(A, p_vxl, i);

      if(iftValidVoxel(label_img, adj_vxl) == true) 
      {
        int adj_idx;

        adj_idx = iftGetVoxelIndex(label_img, adj_vxl);

        if(gt_img->val[p] != gt_img->val[adj_idx]) 
        { is_gt_border = true; }
        if(label_img->val[p] != label_img->val[adj_idx])
        { is_label_border = true; }
      }
			else 
      { is_gt_border = is_label_border = true; } // Limits of the image
    }

    if(is_gt_border == true)
    {
      if(is_label_border == true) { tp++; }
      else { fn++; }
    }
  }
  iftDestroyAdjRel(&A);

  bound_rec = tp/(float)(tp + fn);

  return bound_rec;
}

float iftEvalUE
(iftImage *label_img, iftImage *gt_img)
{
  #ifdef IFT_DEBUG //---------------------------------------------------------|
  assert(label_img != NULL);
  assert(gt_img != NULL);
  iftVerifyImageDomains(label_img, gt_img, __func__);
  #endif //-------------------------------------------------------------------|
  int min_label, max_label, num_labels, min_gt, max_gt, num_gt;
  float under_segm;
  int **inter;

  iftMinMaxValues(label_img, &min_label, &max_label);
  num_labels = max_label - min_label + 1;
  iftMinMaxValues(gt_img, &min_gt, &max_gt);
  num_gt = max_gt - min_gt + 1;

  inter = iftMetrics_CalcLabelGTIntersec(label_img, gt_img);

  under_segm = 0.0;
  #ifdef IFT_OMP //-----------------------------------------------------------|
  #pragma omp parallel for reduction(+:under_segm)
  #endif //-------------------------------------------------------------------|
  for(int i = 0; i < num_labels; ++i)
  {
    int error, size;

    size = 0;
    for(int j = 0; j < num_gt; ++j) { size += inter[i][j]; }

    error = 0;
    for(int j = 0; j < num_gt; ++j)
    {
      if(inter[i][j] > 0) 
      // sum(min{|S_i ^ G_j|, |S_i - G_j|})
      { error += iftMin(inter[i][j], size - inter[i][j]); } 
    }
    under_segm += error;
  }
  for(int i = 0; i < num_labels; ++i) { free(inter[i]); }
  free(inter);

  under_segm /= (float)label_img->n;

  return under_segm;
}
