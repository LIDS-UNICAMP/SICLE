/*****************************************************************************\
* RunMeanLabel.c
*
* AUTHOR  : Felipe Belem
* DATE    : 2021-03-08
* LICENSE : MIT License
* EMAIL   : felipe.belem@ic.unicamp.br
\*****************************************************************************/
#include "ift.h"
#include "iftArgs.h"

void usage();
iftImage *calcPseudoLabel
(iftImage *label_img, iftImage *img, float alpha);
void readImgInputs
(iftArgs *args, iftImage **img, iftImage **labels, const char **path, 
  bool *is_video);

int main(int argc, char const *argv[])
{
  //-------------------------------------------------------------------------//
  bool has_req, has_help;
  iftArgs *args;

  args = iftCreateArgs(argc, argv);

  has_req = iftExistArg(args, "labels") && iftExistArg(args, "out");
  has_help = iftExistArg(args, "help");

  if(!has_req || has_help)
  {
    usage(); 
    iftDestroyArgs(&args);
    return EXIT_FAILURE;
  }
  //-------------------------------------------------------------------------//
  bool is_video;
  const char *OUT_PATH;
  float alpha;
  iftImage *img, *label_img, *out_img;

  readImgInputs(args, &img, &label_img, &OUT_PATH, &is_video);
  alpha = 1.0;
  if(iftExistArg(args, "opac") == true)
  {
    if(iftHasArgVal(args, "opac") == true)
    {
      alpha = atof(iftGetArg(args, "opac"));
      if(alpha < 0 || alpha > 1)
        iftError("Opacity must be within [0,1]", __func__);   
    }
    else iftError("No opacity value was given", __func__);
  }
  iftDestroyArgs(&args);

  out_img = calcPseudoLabel(label_img, img, alpha);
  iftDestroyImage(&label_img);

  if(is_video == false)
  { iftWriteImageByExt(out_img, OUT_PATH); }
  else
  { iftWriteVolumeAsSingleVideoFolder(out_img, OUT_PATH); }
  iftDestroyImage(&out_img);

  return EXIT_SUCCESS;
}

void usage()
{
  const int SKIP_IND = 15; // For indentation purposes
  printf("\nThe required parameters are:\n");
  printf("%-*s %s\n", SKIP_IND, "--labels", 
         "Input label image");
  printf("%-*s %s\n", SKIP_IND, "--out", 
         "Output pseudo colored label image ");

  printf("\nThe optional parameters are:\n");
  printf("%-*s %s\n", SKIP_IND, "--img", 
         "Original image");
  printf("%-*s %s\n", SKIP_IND, "--opac", 
         "Label opacity. Default: 1.0");
  printf("%-*s %s\n", SKIP_IND, "--help", 
         "Prints this message");

  printf("\n");
}

iftImage *calcPseudoLabel
(iftImage *label_img, iftImage *img, float alpha)
{
  #if IFT_DEBUG //-----------------------------------------------------------//
  assert(label_img != NULL);
  if(img != NULL) iftVerifyImageDomains(img, label_img, __func__);
  assert(alpha >= 0 && alpha <= 1);
  #endif //------------------------------------------------------------------//
  const int NORM_VAL = 255, ANGLE_SKIP = 64, SAT_ROUNDS = 5, VAL_ROUNDS = 3;
  const iftColor BLACK_RGB = {{0,0,0},1.0};
  int min_label, max_label, num_labels, hue, round_sat, round_val;
  float sat, val;
  int *label_hsv;
  float *label_saturation, *label_val;
  iftImage *pseudo_img, *tmp_img;

  pseudo_img = iftCreateColorImage(label_img->xsize, label_img->ysize, 
                                   label_img->zsize, 8);

  iftMinMaxValues(label_img, &min_label, &max_label);
  num_labels = max_label - min_label + 1;
  
  label_hsv = calloc(num_labels, sizeof(int));
  assert(label_hsv != NULL);
  label_saturation = calloc(num_labels, sizeof(float));
  assert(label_saturation != NULL);
  label_val = calloc(num_labels, sizeof(float));
  assert(label_val != NULL);

  hue = 0;
  round_sat = round_val = 0;
  sat = val = 0.0;
  for(int i = 0; i < num_labels; ++i)
  {
    
    if(round_sat + 1 == SAT_ROUNDS) round_val = (round_val + 1) % VAL_ROUNDS;
    if(hue + ANGLE_SKIP >= 360) round_sat = (round_sat + 1) % SAT_ROUNDS;

    hue = (hue + ANGLE_SKIP) % 360;
    sat = (SAT_ROUNDS - round_sat)/(float)SAT_ROUNDS;
    val = (VAL_ROUNDS - round_val)/(float)VAL_ROUNDS;

    label_hsv[i] = hue;
    label_saturation[i] = sat;
    label_val[i] = val;
  }

  if(img != NULL)
  {
    int norm_val;

    norm_val = iftNormalizationValue(iftMaximumValue(img));
    tmp_img = iftCopyImage(img);
    if(norm_val != NORM_VAL) iftConvertNewBitDepth(&tmp_img, 8);
  } else tmp_img = NULL;

  #if IFT_OMP //-------------------------------------------------------------//
  #pragma omp parallel for
  #endif //------------------------------------------------------------------//
  for(int p = 0; p < pseudo_img->n; ++p)
  {
    int p_label;
    iftColor HSV, RGB, bkg_rgb, YCbCr;

    p_label = label_img->val[p];

    if(tmp_img != NULL)
    {
      iftColor img_ycbcr;

      if(iftIsColorImage(tmp_img) == true)
      {
        img_ycbcr.val[0] = tmp_img->val[p];
        img_ycbcr.val[1] = tmp_img->Cb[p];
        img_ycbcr.val[2] = tmp_img->Cr[p];
        bkg_rgb = iftYCbCrtoRGB(img_ycbcr, NORM_VAL);
      }
      else
      {
        bkg_rgb.val[0] = tmp_img->val[p];
        bkg_rgb.val[1] = tmp_img->val[p];
        bkg_rgb.val[2] = tmp_img->val[p];
      }
    } else bkg_rgb = BLACK_RGB;

    if(p_label > 0)
    {
      HSV.val[0] = label_hsv[p_label - min_label];
      HSV.val[1] = label_saturation[p_label - min_label] * NORM_VAL;
      HSV.val[2] = label_val[p_label - min_label] * NORM_VAL;

      RGB = iftHSVtoRGB(HSV, NORM_VAL);

      RGB.val[0] = (RGB.val[0] * alpha) + (bkg_rgb.val[0] * (1.0 - alpha));
      RGB.val[1] = (RGB.val[1] * alpha) + (bkg_rgb.val[1] * (1.0 - alpha));
      RGB.val[2] = (RGB.val[2] * alpha) + (bkg_rgb.val[2] * (1.0 - alpha));
      YCbCr = iftRGBtoYCbCr(RGB, NORM_VAL);

      pseudo_img->val[p] = YCbCr.val[0];
      pseudo_img->Cb[p] = YCbCr.val[1];
      pseudo_img->Cr[p] = YCbCr.val[2];
    }
    else { RGB = bkg_rgb; }
    YCbCr = iftRGBtoYCbCr(RGB, NORM_VAL);

    pseudo_img->val[p] = YCbCr.val[0];
    pseudo_img->Cb[p] = YCbCr.val[1];
    pseudo_img->Cr[p] = YCbCr.val[2];
  }

  free(label_hsv);
  free(label_saturation);
  if(img != NULL) iftDestroyImage(&tmp_img);

  return pseudo_img;
}

void readImgInputs
(iftArgs *args, iftImage **img, iftImage **labels, const char **path, 
  bool *is_video)
{
  #if IFT_DEBUG //-----------------------------------------------------------//
  assert(args != NULL);
  assert(labels != NULL);
  assert(path != NULL);
  #endif //------------------------------------------------------------------//
  const char *PATH;

  if(iftHasArgVal(args, "labels") == true) 
  {
    PATH = iftGetArg(args, "labels");

    if(iftIsImageFile(PATH) == true) 
    { (*labels) = iftReadImageByExt(PATH); (*is_video) = false; }
    else if(iftDirExists(PATH) == true)
    { (*labels) = iftReadImageFolderAsVolume(PATH); (*is_video) = true;}
    else { iftError("Unknown image/video format", __func__); } 
  }
  else iftError("No label image path was given", __func__);

  if(iftHasArgVal(args, "out") == true)
  { (*path) = iftGetArg(args, "out"); }
  else iftError("No output image path was given", __func__);

  if(iftExistArg(args, "img") == true)
  {
    if(iftHasArgVal(args, "img") == true) 
    {
      PATH = iftGetArg(args, "img");

      if(iftIsImageFile(PATH) == true) 
      { (*img) = iftReadImageByExt(PATH); (*is_video) = false; }
      else if(iftDirExists(PATH) == true)
      { (*img) = iftReadImageFolderAsVolume(PATH); (*is_video) = true;}
      else { iftError("Unknown image/video format", __func__); } 
    }
    else iftError("No original image path was given", __func__);
  }
  else (*img) = NULL;
}