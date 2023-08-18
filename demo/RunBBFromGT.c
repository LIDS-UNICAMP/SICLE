/*****************************************************************************\
* RunBBFromGT.c
*
* AUTHOR  : Felipe Belem
* DATE    : 2021-07-20
* LICENSE : MIT License
* EMAIL   : felipe.belem@ic.unicamp.br
\*****************************************************************************/
#include "ift.h"
#include "iftArgs.h"

/* HEADERS __________________________________________________________________*/
void usage();
void readImgInputs
(iftArgs *args, iftImage **gt_img, const char **path, bool *is_video);

/* MAIN _____________________________________________________________________*/
int main(int argc, char const *argv[])
{
  /*-------------------------------------------------------------------------*/
  bool has_req, has_help;
  iftArgs *args;

  args = iftCreateArgs(argc, argv);
  has_req = iftExistArg(args, "gt") && iftExistArg(args, "out");
  has_help = iftExistArg(args, "help");

  if(!has_req || has_help)
  { usage(); iftDestroyArgs(&args); return EXIT_FAILURE; }
  /*-------------------------------------------------------------------------*/
  const char *OUT_PATH;
  bool is_video;
  int min_val;
  iftBoundingBox bb;
  iftImage *gt_img, *out_img;

  readImgInputs(args, &gt_img, &OUT_PATH, &is_video);
  iftDestroyArgs(&args);
  
  min_val = iftMinimumValue(gt_img);
  #if IFT_OMP /*#############################################################*/
  #pragma omp parallel for
  #endif /*##################################################################*/
  for(int p_idx = 0; p_idx < gt_img->n; ++p_idx)
  { if(gt_img->val[p_idx] == min_val) { gt_img->val[p_idx] = 0; } }

  bb = iftMinBoundingBox(gt_img, NULL);
  out_img = iftCreateImage(gt_img->xsize, gt_img->ysize, gt_img->zsize);
  iftDestroyImage(&gt_img);
  
  #if IFT_OMP /*#############################################################*/
  #pragma omp parallel for
  #endif /*##################################################################*/
  for(int p_z = bb.begin.z; p_z <= bb.end.z; ++p_z)
  {
    for(int p_y = bb.begin.y; p_y <= bb.end.y; ++p_y)
    {
      for(int p_x = bb.begin.x; p_x <= bb.end.x; ++p_x)
      {
        int p_idx;
        iftVoxel p_vxl;
        
        p_vxl.x = p_x; p_vxl.y = p_y; p_vxl.z = p_z;
        p_idx = iftGetVoxelIndex(out_img, p_vxl);
        out_img->val[p_idx] = 255;
      }
    }
  }

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
  printf("%-*s %s\n", SKIP_IND, "--gt", 
         "Input groundtruth image/video folder");
  printf("%-*s %s\n", SKIP_IND, "--out", 
         "Output groundtruth bounding-box image/video folder");

  printf("\nThe optional parameters are:\n");
  printf("%-*s %s\n", SKIP_IND, "--help", 
         "Prints this message");

  printf("\n");
}

void readImgInputs
(iftArgs *args, iftImage **gt_img, const char **path, bool *is_video)
{
  #if IFT_DEBUG /*###########################################################*/
  assert(args != NULL);
  assert(gt_img != NULL);
  #endif /*##################################################################*/
  const char *PATH;

  if(iftHasArgVal(args, "gt")) 
  {
    PATH = iftGetArg(args, "gt");

    if(iftIsImageFile(PATH)) 
    { (*gt_img) = iftReadImageByExt(PATH); (*is_video) = false; }
    else if(iftDirExists(PATH))
    { (*gt_img) = iftReadImageFolderAsVolume(PATH); (*is_video) = true; }
    else { iftError("Unknown image/video format", __func__); } 
  }
  else { iftError("No ground-truth image path was given", __func__); }

  if(iftHasArgVal(args, "out"))
  { (*path) = iftGetArg(args, "out"); }
  else { iftError("No output image path was given", __func__); }
}

