/*****************************************************************************\
* iftMetrics.c
*
* AUTHOR  : Felipe Belem
* DATE    : 2021-03-08
* LICENSE : MIT License
* EMAIL   : felipe.belem@ic.unicamp.br
\*****************************************************************************/
#include "iftMetrics.h"

/*****************************************************************************\
*
*                              PRIVATE FUNCTIONS
*
\*****************************************************************************/
//===========================================================================//
// GENERAL & AUXILIARY
//===========================================================================//
/*
  Counts the number of spels in which each label intersects with an object 
  defined in the ground-truth
*/
int **_iftCalcLabelGTIntersec
(const iftImage *label_img, const iftImage *gt_img)
{
  #ifdef IFT_DEBUG //--------------------------------------------------------//
  assert(label_img != NULL);
  assert(gt_img != NULL);
  iftVerifyImageDomains(label_img, gt_img, "_iftCalcLabelGTIntersec");
  #endif //------------------------------------------------------------------//
  int min_label, max_label, num_labels, min_gt, max_gt, num_gt;
  int **inter;

  iftMinMaxValues(label_img, &min_label, &max_label);
  num_labels = max_label - min_label + 1;
  iftMinMaxValues(gt_img, &min_gt, &max_gt);
  num_gt = max_gt - min_gt + 1;

  inter = calloc(num_labels, sizeof(int*));
  assert(inter != NULL);

  for(int i = 0; i < num_labels; ++i) // For each label
  {
    inter[i] = calloc(num_gt, sizeof(int));
    assert(inter[i] != NULL);
  }

  for(int p = 0; p < label_img->n; ++p) // For each spel
  {
    int label, gt;

    label = label_img->val[p] - min_label;
    gt = gt_img->val[p] - min_gt;

    inter[label][gt]++; // Add one more spel to the intersection
  }

  return inter;
}

/*****************************************************************************\
*
*                               PUBLIC FUNCTIONS
*
\*****************************************************************************/
//===========================================================================//
// NON GT-BASED
//===========================================================================//
float iftExplVaria
(const iftImage *orig_img, const iftImage *label_img)
{
  #ifdef IFT_DEBUG //--------------------------------------------------------//
  assert(orig_img != NULL);
  assert(label_img != NULL);
  iftVerifyImageDomains(orig_img, label_img, "iftExplVaria");
  #endif //------------------------------------------------------------------//
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
  
  for(int i = 0; i < num_labels; ++i) // For each label
  {
    mean_sup[i] = calloc(num_feats, sizeof(float));
    assert(mean_sup[i] != NULL);  
  }

  // Get the image's and the superspels' mean feature vector
  for(int p = 0; p < orig_img->n; ++p)  // For each spel
  {
    int p_label;

    p_label = label_img->val[p] - min_label;

    mean_img[0] += orig_img->val[p];          // Add the luminosity
    mean_sup[p_label][0] += orig_img->val[p]; //
    if(num_feats > 1) // If it is colored
    {
      mean_img[1] += orig_img->Cb[p];          // Add the Cb
      mean_sup[p_label][1] += orig_img->Cb[p]; //

      mean_img[2] += orig_img->Cr[p];          // Add the Cr
      mean_sup[p_label][2] += orig_img->Cr[p]; //
    }

    sup_size[p_label]++; // Add one more spel to the superspel count
  }

  // Compute the respective mean feature vector
  for(int j = 0; j < num_feats; ++j) // For each feature
  {
    mean_img[j] /= (float)orig_img->n; // Divide by the image's size
    for(int i = 0; i < num_labels; ++i) // For each label
      mean_sup[i][j] /= (float)sup_size[i]; // Divide by its own size
  }

  img_diff = 0.0; // i.e., (p_i - mean_img)^2
  #ifdef IFT_OMP //----------------------------------------------------------//
  #pragma omp parallel for reduction(+:img_diff)
  #endif //------------------------------------------------------------------//
  for(int p = 0; p < orig_img->n; ++p) // For each spel
  {
    float dist;

    dist = orig_img->val[p] - mean_img[0];//Calculate the luminosity difference
    if(num_feats > 1) // If it is colored
    {
      dist += orig_img->Cb[p] - mean_img[1]; // Calculate the Cb difference
      dist += orig_img->Cr[p] - mean_img[2]; // Calculate the Cr difference
    }

    img_diff += (dist * dist); // Calculate the squared distance
  }

  expl_var = 0.0;
  #ifdef IFT_OMP //----------------------------------------------------------//
  #pragma omp parallel for reduction(+:expl_var)
  #endif //------------------------------------------------------------------//
  for(int i = 0; i < num_labels; ++i) // For each label
  {
    float dist;

    dist = 0; // (mean_sup_i - mean_img)^2
    for(int j = 0; j < num_feats; ++j) // For each feature
      dist += (mean_sup[i][j] - mean_img[j]); // Calculate the difference

    expl_var += sup_size[i] * (dist * dist); // Accumulate the squared distance
  }
  expl_var /= img_diff; // Normalize to [0,1]

  for(int i = 0; i < num_labels; ++i)
    free(mean_sup[i]);
  free(mean_sup);
  free(sup_size);
  free(mean_img);

  return expl_var;
}

float iftTempExt
(const iftImage *label_img)
{
  #ifdef IFT_DEBUG //--------------------------------------------------------//
  assert(label_img != NULL);
  #endif //------------------------------------------------------------------//
  int min_label, max_label, num_labels;
  float temp_ext;
  int *first_frame, *last_frame;

  iftMinMaxValues(label_img, &min_label, &max_label);
  num_labels = max_label - min_label + 1;

  first_frame = calloc(num_labels, sizeof(int));
  assert(first_frame != NULL);
  last_frame = calloc(num_labels, sizeof(int));
  assert(last_frame != NULL);

  for(int i = 0; i < num_labels; ++i) // For each label
  {
    first_frame[i] = label_img->zsize - 1; // It is expected that all labels 
    last_frame[i] = 0;                     // has at least one spel in a frame
  }

  for(int p = 0; p < label_img->n; ++p) // For each spel
  {
    int p_frame, p_label;

    p_frame = iftGetZCoord(label_img, p); // For video, Z is the frame
    p_label = label_img->val[p] - min_label;

    // If the current label frame is closer to the 1st frame
    if(first_frame[p_label] > p_frame) first_frame[p_label] = p_frame;
    // If the current label frame is closer to the last frame
    if(last_frame[p_label] < p_frame) last_frame[p_label] = p_frame;
  }
  
  temp_ext = 0.0;
  for(int i = 0; i < num_labels; ++i) // For each label
    temp_ext += last_frame[i] - first_frame[i];//Accumulate the number of frames
  temp_ext /= (float)(label_img->zsize * num_labels); // Normalize to [0,1]

  free(first_frame);
  free(last_frame);

  return temp_ext;
}

//===========================================================================//
// GT-BASED
//===========================================================================//
float iftAchiSegmAccur
(const iftImage *label_img, const iftImage *gt_img)
{
  #ifdef IFT_DEBUG //--------------------------------------------------------//
  assert(label_img != NULL);
  assert(gt_img != NULL);
  iftVerifyImageDomains(label_img, gt_img, "iftAchiSegmAccur");
  #endif //------------------------------------------------------------------//
  int min_label, max_label, num_labels, min_gt, max_gt, num_gt;
  float achi_segm;
  int **inter;

  iftMinMaxValues(label_img, &min_label, &max_label);
  num_labels = max_label - min_label + 1;
  iftMinMaxValues(gt_img, &min_gt, &max_gt);
  num_gt = max_gt - min_gt + 1;

  // Calculate the intersection between the superspels and the GT objects
  inter = _iftCalcLabelGTIntersec(label_img, gt_img);

  achi_segm = 0.0;
  #ifdef IFT_OMP //----------------------------------------------------------//
  #pragma omp parallel for reduction(+:achi_segm)
  #endif //------------------------------------------------------------------//
  for(int i = 0; i < num_labels; ++i)
  {
    int max_inter;

    max_inter = 0;
    for(int j = 0; j < num_gt; ++j) // For each GT object
      // Get the one with higher intersection (i.e., higher number of spels)
      if(inter[i][j] > max_inter) max_inter = inter[i][j];

    achi_segm += max_inter; // Accumulate the best intersection with this label
  }
  achi_segm /= (float)label_img->n; // Normalize to [0,1]

  for(int i = 0; i < num_labels; ++i)
    free(inter[i]);
  free(inter);

  return achi_segm;
}

float iftBoundRecall
(const iftImage *label_img, const iftImage *gt_img)
{
  #ifdef IFT_DEBUG //--------------------------------------------------------//
  assert(label_img != NULL);
  assert(gt_img != NULL);
  iftVerifyImageDomains(label_img, gt_img, "iftBoundRecall");
  #endif //------------------------------------------------------------------//
  int tp, fn, r;
  float bound_rec; 
  iftAdjRel *A;

  r = ceil(0.0025 * iftDiagonalSize(label_img)); // See [1] for more details
  // The adjacency below is equivalent to a square neighborhood of 2r+1 x 2r+1
  A = iftCircular(r * sqrtf(2.0)); // Both video and image (not 3D) use this

  tp = fn = 0;
  #ifdef IFT_OMP //----------------------------------------------------------//
  #pragma omp parallel for reduction(+:tp, fn)
  #endif //------------------------------------------------------------------//
  for(int p = 0; p < label_img->n; ++p) // For each spel
  {
    bool is_label_border, is_gt_border;
    iftVoxel p_vxl;
    
    p_vxl = iftGetVoxelCoord(label_img, p);

    is_gt_border = is_label_border = false;//Set as a non-border spel(for now)
    for(int i = 1; i < A->n; ++i) // For each adjacent spel (i = 0 == p)
    {
      iftVoxel adj_vxl;

      adj_vxl = iftGetAdjacentVoxel(A, p_vxl, i);

      // If it is within the image domain
      if(iftValidVoxel(label_img, adj_vxl) == true) 
      {
        int adj_idx;

        adj_idx = iftGetVoxelIndex(label_img, adj_vxl);

        // If in the GT image, p and adj have different labels
        if(gt_img->val[p] != gt_img->val[adj_idx]) is_gt_border = true;
        // If in the label image, p and adj have different labels
        if(label_img->val[p] != label_img->val[adj_idx]) is_label_border = true;
      }
    }

    if(is_gt_border == true) // If p is a GT border spel
    {
      // If it is also a border spel in the label image
      if(is_label_border == true) tp++; // It was correctly assigned as border
      else fn++; // It was incorrectly assigned as non-border
    } // Then, don't care for true negatives or false positives 
  }

  bound_rec = tp/(float)(tp + fn); // Compute recall

  iftDestroyAdjRel(&A);

  return bound_rec;
}

float iftUnderSegmError
(const iftImage *label_img, const iftImage *gt_img)
{
  #ifdef IFT_DEBUG //--------------------------------------------------------//
  assert(label_img != NULL);
  assert(gt_img != NULL);
  iftVerifyImageDomains(label_img, gt_img, "iftUnderSegmError");
  #endif //------------------------------------------------------------------//
  int min_label, max_label, num_labels, min_gt, max_gt, num_gt;
  float under_segm;
  int **inter;

  iftMinMaxValues(label_img, &min_label, &max_label);
  num_labels = max_label - min_label + 1;
  iftMinMaxValues(gt_img, &min_gt, &max_gt);
  num_gt = max_gt - min_gt + 1;

  // Calculate the intersection between the superspels and the GT objects
  inter = _iftCalcLabelGTIntersec(label_img, gt_img);

  under_segm = 0.0;
  #ifdef IFT_OMP //----------------------------------------------------------//
  #pragma omp parallel for reduction(+:under_segm)
  #endif //------------------------------------------------------------------//
  for(int i = 0; i < num_labels; ++i) // For each label
  {
    int error, size;

    size = 0;
    for(int j = 0; j < num_gt; ++j) // Get the label's size (in spels)
      size += inter[i][j]; // Accumulate its intersection with the GT object

    error = 0;
    for(int j = 0; j < num_gt; ++j) // For each GT object
    {
      if(inter[i][j] > 0)//If exists at least one spel in their intersection
        // Gets the lowest error between inner and outer leakings
        // |Sj - Gi| = |Sj| - |Sj intersec Gi| in this case
        error += iftMin(inter[i][j], size - inter[i][j]);
    }

    under_segm += error; // Accumulate the label's leaking errors
  }
  under_segm /= (float)label_img->n; // Normalize to [0,1]

  for(int i = 0; i < num_labels; ++i)
    free(inter[i]);
  free(inter);

  return under_segm;
}
