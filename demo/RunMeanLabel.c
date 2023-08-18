/*****************************************************************************\
* RunMeanLabel.c
*
* AUTHOR  : Felipe Belem
* DATE    : 2021-02-28
* LICENSE : MIT License
* EMAIL   : felipe.belem@ic.unicamp.br
\*****************************************************************************/
#include "ift.h"
#include "iftArgs.h"

/* HEADERS __________________________________________________________________*/
void usage();
void readImgInputs
(iftArgs *args, iftImage **img, iftImage **labels, const char **path, 
  bool *is_video);
iftImage *calcMeanLabel
(iftImage *orig_img, iftImage *label_img);

/* MAIN _____________________________________________________________________*/
int main(int argc, char const *argv[])
{
  /*-------------------------------------------------------------------------*/
  bool has_req, has_help;
  iftArgs *args;

  args = iftCreateArgs(argc, argv);
  has_req = iftExistArg(args, "img")  && iftExistArg(args, "labels") &&
            iftExistArg(args, "out");
  has_help = iftExistArg(args, "help");

  if(!has_req || has_help)
  { usage(); iftDestroyArgs(&args); return EXIT_FAILURE; }
  /*-------------------------------------------------------------------------*/
  const char *OUT_PATH;
  bool is_video;
  iftImage *img, *label_img, *out_img;

  readImgInputs(args, &img, &label_img, &OUT_PATH, &is_video);
  iftDestroyArgs(&args);

  out_img = calcMeanLabel(img, label_img);
  iftDestroyImage(&img);
  iftDestroyImage(&label_img);

  if(!is_video)
  { iftWriteImageByExt(out_img, OUT_PATH); }
  else
  { iftWriteVolumeAsSingleVideoFolder(out_img, OUT_PATH); }
  iftDestroyImage(&out_img);

  return EXIT_SUCCESS;
}

/* DEFINITIONS ______________________________________________________________*/
void usage()
{
  const int SKIP_IND = 15; // For indentation purposes
  printf("\nThe required parameters are:\n");
  printf("%-*s %s\n", SKIP_IND, "--img", 
         "Input image");
  printf("%-*s %s\n", SKIP_IND, "--labels", 
         "Input label");
  printf("%-*s %s\n", SKIP_IND, "--out", 
         "Output mean label colored image");

  printf("\nThe optional parameters are:\n");
  printf("%-*s %s\n", SKIP_IND, "--help", 
         "Prints this message");

  printf("\n");
}

void readImgInputs
(iftArgs *args, iftImage **img, iftImage **labels, const char **path,
  bool *is_video)
{
  #if IFT_DEBUG /*###########################################################*/
  assert(args != NULL);
  assert(img != NULL);
  assert(labels != NULL);
  assert(path != NULL);
  #endif /*##################################################################*/
  const char *PATH;

  if(iftHasArgVal(args, "img")) 
  {
    PATH = iftGetArg(args, "img");

    if(iftIsImageFile(PATH)) 
    { (*img) = iftReadImageByExt(PATH); (*is_video) = false; }
    else if(iftDirExists(PATH))
    { (*img) = iftReadImageFolderAsVolume(PATH); (*is_video) = true;}
    else { iftError("Unknown image/video format", __func__); } 
  }
  else iftError("No image path was given", __func__);

  if(iftHasArgVal(args, "labels")) 
  {
    PATH = iftGetArg(args, "labels");

    if(iftIsImageFile(PATH)) 
    { (*labels) = iftReadImageByExt(PATH);}
    else if(iftDirExists(PATH))
    { (*labels) = iftReadImageFolderAsVolume(PATH);}
    else { iftError("Unknown image/video format", __func__); }
  }
  else iftError("No label image path was given", __func__);

  iftVerifyImageDomains(*img, *labels, __func__);

  if(iftHasArgVal(args, "out") == true)
  { (*path) = iftGetArg(args, "out"); }
  else iftError("No output image path was given", __func__);
}

iftImage *calcMeanLabel
(iftImage *orig_img, iftImage *label_img)
{
  #if IFT_DEBUG /*###########################################################*/
  assert(orig_img != NULL);
  assert(label_img != NULL);
  iftVerifyImageDomains(orig_img, label_img, __func__);
  #endif /*##################################################################*/
  int min_label, max_label, num_labels, num_feats;
  int *label_size;
  float **label_feats;

  iftMinMaxValues(label_img, &min_label, &max_label);
  num_labels = max_label - min_label + 1;
  
  num_feats = iftIsColorImage(orig_img) ? 3 : 1;
  label_size = calloc(num_labels, sizeof(int));
  assert(label_size != NULL);
  label_feats = calloc(num_labels, sizeof(float*));
  assert(label_feats != NULL);

  for(int i = 0; i < num_labels; ++i)
  {
    label_size[i] = 0;
    label_feats[i] = calloc(num_feats, sizeof(float));
    assert(label_feats[i]);
  }

  for(int p_idx = 0; p_idx < orig_img->n; ++p_idx)
  {
    int p_label;

    p_label = label_img->val[p_idx];

    label_size[p_label - min_label]++;
    label_feats[p_label - min_label][0] += orig_img->val[p_idx];
    if(num_feats > 1)
    {
      label_feats[p_label - min_label][1] += orig_img->Cb[p_idx];
      label_feats[p_label - min_label][2] += orig_img->Cr[p_idx];
    }
  }

  for(int i = 0; i < num_labels; ++i)
  {
    for(int j = 0; j < num_feats; ++j)
    { label_feats[i][j] /= (float)label_size[i]; }
  }
  free(label_size);
  /*-------------------------------------------------------------------------*/
  iftImage *mean_img;

  mean_img = iftCreateImageFromImage(orig_img);
  #if IFT_OMP /*#############################################################*/
  #pragma omp parallel for
  #endif /*##################################################################*/
  for(int p_idx = 0; p_idx < mean_img->n; ++p_idx)
  {
    int p_label;

    p_label = label_img->val[p_idx];

    mean_img->val[p_idx] = label_feats[p_label - min_label][0];
    if(num_feats > 1)
    {
      mean_img->Cb[p_idx] = label_feats[p_label - min_label][1];
      mean_img->Cr[p_idx] = label_feats[p_label - min_label][2];
    }
  }
  for(int i = 0; i < num_labels; ++i) { free(label_feats[i]); }
  free(label_feats);

  if(iftImageDepth(orig_img) != 8) { iftConvertNewBitDepth(&mean_img, 8); }

  return mean_img;
}
