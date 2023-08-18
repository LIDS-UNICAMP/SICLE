/*****************************************************************************\
* RunOvlayBorders.c
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
void readOptArgs
(iftArgs *args, float *thick, iftFColor *rgb);
iftImage *ovlayBorders
(iftImage *orig_img, iftImage *label_img, float thick, iftFColor rgb);

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
  bool is_video;
  const char *OVLAY_PATH;
  float thick;
  iftImage *img, *label_img, *ovlay_img;
  iftFColor rgb;

  readImgInputs(args, &img, &label_img, &OVLAY_PATH, &is_video);
  readOptArgs(args, &thick, &rgb);
  iftDestroyArgs(&args);

  ovlay_img = ovlayBorders(img, label_img, thick, rgb);
  iftDestroyImage(&img);
  iftDestroyImage(&label_img);

  if(!is_video)
  { iftWriteImageByExt(ovlay_img, OVLAY_PATH); }
  else
  { iftWriteVolumeAsSingleVideoFolder(ovlay_img, OVLAY_PATH); }
  iftDestroyImage(&ovlay_img);

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
         "Input label image");
  printf("%-*s %s\n", SKIP_IND, "--out", 
         "Output border overlayed image");

  printf("\nThe optional parameters are:\n");
  printf("%-*s %s\n", SKIP_IND, "--rgb", 
         "Comma-separated normalized RGB border color. Default: 0,0,0");
  printf("%-*s %s\n", SKIP_IND, "--thick", 
         "Border thickness. Default: 1.0");
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
    { (*img) = iftReadImageFolderAsVolume(PATH); (*is_video) = true; }
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

  if(iftHasArgVal(args, "out"))
  { (*path) = iftGetArg(args, "out"); }
  else iftError("No output image path was given", __func__);
}

void readOptArgs
(iftArgs *args, float *thick, iftFColor *rgb)
{
  #if IFT_DEBUG /*###########################################################*/
  assert(args != NULL);
  assert(thick != NULL);
  assert(rgb != NULL);
  #endif /*##################################################################*/
  
  if(iftExistArg(args, "thick"))
  {
    if(iftHasArgVal(args, "thick")) 
    { (*thick) = atoi(iftGetArg(args, "thick")); }
    else iftError("No border thickness was given", __func__);
  }
  else (*thick) = 1.0;

  if(iftExistArg(args, "rgb"))
  {
    if(iftHasArgVal(args, "rgb"))
    {
      const char *VAL;
      int i;
      char *tmp, *tok;


      VAL = iftGetArg(args, "rgb");
      tmp = iftCopyString(VAL);
      tok = strtok(tmp, ",");

      i = 0;
      while(tok != NULL && i < 3)
      {
        float c;

        c = atof(tok);
        if(c >= 0 && c <= 1) { (*rgb).val[i] = c; }
        else { iftError("The color should be within [0,1]", __func__); }

        tok = strtok(NULL, ","); i++;
      }

      if((tok != NULL && i == 3) || (tok == NULL && i < 2))
      { iftError("Three colors are required for the RGB", __func__); }
      free(tmp);
    }
    else { iftError("No normalized RGB color was given", __func__); }
  }
  else { (*rgb).val[0] = (*rgb).val[1] = (*rgb).val[2] = 0.0; }
}

iftImage *ovlayBorders
(iftImage *orig_img, iftImage *label_img, float thick, iftFColor rgb)
{
  #if IFT_DEBUG /*###########################################################*/
  assert(orig_img != NULL);
  assert(label_img != NULL);
  iftVerifyImageDomains(orig_img, label_img, "ovlayBorders");
  assert(thick > 0);
  #endif /*##################################################################*/
  int depth, norm_val;
  iftImage *ovlay_img;
  iftAdjRel *A;
  iftColor RGB, YCbCr;

  A = iftCircular(thick);

  depth = iftImageDepth(orig_img);
  norm_val = iftMaxImageRange(depth);
  ovlay_img = iftCreateColorImage(orig_img->xsize, orig_img->ysize, 
                                  orig_img->zsize, depth);

  RGB.val[0] = rgb.val[0] * norm_val;
  RGB.val[1] = rgb.val[1] * norm_val;
  RGB.val[2] = rgb.val[2] * norm_val;
  YCbCr = iftRGBtoYCbCr(RGB, norm_val);

  #if IFT_OMP /*#############################################################*/
  #pragma omp parallel for
  #endif /*##################################################################*/
  for(int p_idx = 0; p_idx < ovlay_img->n; ++p_idx)
  {
    bool is_border;
    int i;
    iftVoxel p_vxl;

    is_border = false;
    p_vxl = iftGetVoxelCoord(ovlay_img, p_idx);

    i = 0;
    while(!is_border && i < A->n)
    {
      iftVoxel q_vxl;

      q_vxl = iftGetAdjacentVoxel(A, p_vxl, i);

      if(iftValidVoxel(ovlay_img, q_vxl))
      {
        int q_idx;

        q_idx = iftGetVoxelIndex(ovlay_img, q_vxl);

        if(label_img->val[p_idx] != label_img->val[q_idx]) 
        { is_border = true; }
      }

      ++i;
    }

    if(is_border)
    {
      ovlay_img->val[p_idx] = YCbCr.val[0];
      ovlay_img->Cb[p_idx] = YCbCr.val[1];
      ovlay_img->Cr[p_idx] = YCbCr.val[2];
    }
    else
    {
      ovlay_img->val[p_idx] = orig_img->val[p_idx];
      if(iftIsColorImage(orig_img) == true)
      {
        ovlay_img->Cb[p_idx] = orig_img->Cb[p_idx];
        ovlay_img->Cr[p_idx] = orig_img->Cr[p_idx];
      }

    }
  }
  iftDestroyAdjRel(&A);

  if(depth != 8) { iftConvertNewBitDepth(&ovlay_img, 8); }

  return ovlay_img;
}

