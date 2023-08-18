/*****************************************************************************\
* RunSuperpixelMetrics.c
*
* AUTHOR  : Felipe Belem
* DATE    : 2021-06-15
* LICENSE : MIT License
* EMAIL   : felipe.belem@ic.unicamp.br
\*****************************************************************************/
#include "ift.h"
#include "iftArgs.h"
#include "iftMetrics.h"

void usage();
void readImgInputs
(const iftArgs *args, iftImage **label_img,  iftImage **img, iftImage **gt_img);

int main(int argc, char const *argv[])
{
  //-------------------------------------------------------------------------//
  bool has_req, has_help;
  iftArgs *args;

  args = iftCreateArgs(argc, argv);

  has_req = iftExistArg(args, "labels");
  has_help = iftExistArg(args, "help");

  if(has_req == false || has_help == true)
  {
    usage(); 
    iftDestroyArgs(&args);
    return EXIT_FAILURE;
  }
  //-------------------------------------------------------------------------//
  bool as_csv;
  int min_label, max_label, num_labels;
  char out_str[IFT_STR_DEFAULT_SIZE], tmp_str[IFT_STR_DEFAULT_SIZE];
  iftImage *label_img, *img, *gt_img;

  readImgInputs(args, &label_img, &img, &gt_img);

  iftImage *relabel_img = iftRelabelImage(label_img);
  iftDestroyImage(&label_img);
  label_img = relabel_img;

  iftMinMaxValues(label_img, &min_label, &max_label);
  num_labels = max_label - min_label + 1;

  sprintf(out_str, "");
  as_csv = iftExistArg(args, "csv");
  if(as_csv == false) sprintf(tmp_str, "K: %d\n", num_labels);
  else sprintf(tmp_str, "%d", num_labels);
  strcat(out_str, tmp_str);

	if(iftExistArg(args, "all") || iftExistArg(args, "asa"))
	{
		float asa = iftEvalASA(label_img, gt_img);
		if(!as_csv){ sprintf(tmp_str, "ASA(+): %.3f\n", asa);  }
		else { sprintf(tmp_str, ",%f", asa); }
		strcat(out_str, tmp_str);
	}

	if(iftExistArg(args, "all") || iftExistArg(args, "br"))
	{
		float br = iftEvalBR(label_img, gt_img);
		if(!as_csv){ sprintf(tmp_str, "BR(+): %.3f\n", br);  }
		else { sprintf(tmp_str, ",%f", br); }
		strcat(out_str, tmp_str);
	}

	if(iftExistArg(args, "all") || iftExistArg(args, "cd"))
	{
		float cd = iftEvalCD(label_img);
		if(!as_csv){ sprintf(tmp_str, "CD(-): %.3f\n", cd);  }
		else { sprintf(tmp_str, ",%f", cd); }
		strcat(out_str, tmp_str);
	}
  
	if(iftExistArg(args, "all") || iftExistArg(args, "co"))
	{
		float co = iftEvalCO(label_img);
		if(!as_csv){ sprintf(tmp_str, "CO(+): %.3f\n", co);  }
		else { sprintf(tmp_str, ",%f", co); }
		strcat(out_str, tmp_str);
	}

	if(iftExistArg(args, "all") || iftExistArg(args, "ev"))
	{
		float ev = iftEvalEV(label_img, img);
		if(!as_csv){ sprintf(tmp_str, "EV(+): %.3f\n", ev);  }
		else { sprintf(tmp_str, ",%f", ev); }
		strcat(out_str, tmp_str);
	}

	if(iftExistArg(args, "all") || iftExistArg(args, "tex"))
	{
		float tex = iftEvalTEX(label_img);
		if(!as_csv){ sprintf(tmp_str, "TEX(+): %.3f\n", tex);  }
		else { sprintf(tmp_str, ",%f", tex); }
		strcat(out_str, tmp_str);
	}

	if(iftExistArg(args, "all") || iftExistArg(args, "ue"))
	{
		float ue = iftEvalUE(label_img, gt_img);
		if(!as_csv){ sprintf(tmp_str, "UE(-): %.3f\n", ue);  }
		else { sprintf(tmp_str, ",%f", ue); }
		strcat(out_str, tmp_str);
	}

  iftDestroyArgs(&args);
  iftDestroyImage(&label_img);
  if(gt_img != NULL) iftDestroyImage(&gt_img);
  if(img != NULL) iftDestroyImage(&img);

  puts(out_str);
  return EXIT_SUCCESS;
}

void usage()
{
  const int SKIP_IND = 15; // For indentation purposes
  printf("\nThe required parameters are:\n");
  printf("%-*s %s\n", SKIP_IND, "--labels", 
         "Input label image/video folder");

  printf("\nThe optional parameters are:\n");
  printf("%-*s %s\n", SKIP_IND, "--csv", 
         "Flag for printing the output as comma-separated values (CSV).");
  printf("%-*s %s\n", SKIP_IND, "--gt", 
         "Input ground-truth image/video folder.");
  printf("%-*s %s\n", SKIP_IND, "--img", 
         "Input original image/video folder.");
  printf("%-*s %s\n", SKIP_IND, "--help", 
         "Prints this message");

  printf("\nThe evaluation metrics are:\n");
  printf("%-*s %s\n", SKIP_IND, "--all", 
         "Computes all metrics. Ground-truth and original image needed.");
  printf("%-*s %s\n", SKIP_IND, "--asa", 
         "Computes the Achievable Segmentation Accuracy (ASA). Ground-truth needed.");
  printf("%-*s %s\n", SKIP_IND, "--br", 
         "Computes the Boundary Recall (BR). Ground-truth needed.");
  printf("%-*s %s\n", SKIP_IND, "--cd", 
         "Computes the Contour Density (CD).");
  printf("%-*s %s\n", SKIP_IND, "--co", 
         "Computes the Compactness (CO).");
  printf("%-*s %s\n", SKIP_IND, "--ev", 
         "Computes the Explained Variation (EV). Original image needed.");
  printf("%-*s %s\n", SKIP_IND, "--tex", 
         "Computes the Temporal EXtension (TEX).");
  printf("%-*s %s\n", SKIP_IND, "--ue", 
         "Computes the Under-segmentation Error (UE). Ground-truth needed.");

  printf("\n");
}

void readImgInputs
(const iftArgs *args, iftImage **label_img,  iftImage **img, iftImage **gt_img)
{
  #if IFT_DEBUG //-----------------------------------------------------------//
  assert(args != NULL);
  assert(img != NULL);
  assert(label_img != NULL);
  assert(gt_img != NULL);
  #endif //------------------------------------------------------------------//
  const char *PATH;

  if(iftHasArgVal(args, "labels") == true) 
  {
    PATH = iftGetArg(args, "labels");

    if(iftIsImageFile(PATH) == true) 
    { (*label_img) = iftReadImageByExt(PATH);}
    else if(iftDirExists(PATH) == true)
    { (*label_img) = iftReadImageFolderAsVolume(PATH);}
    else { iftError("Unknown image/video format", __func__); }
  }
  else iftError("No label image path was given", "readImgInputs");

  if(iftExistArg(args, "img") == true)
  {
    if(iftHasArgVal(args, "img") == true) 
    {
      PATH = iftGetArg(args, "img");

      if(iftIsImageFile(PATH) == true) 
      { (*img) = iftReadImageByExt(PATH);}
      else if(iftDirExists(PATH) == true)
      { (*img) = iftReadImageFolderAsVolume(PATH);}
      else { iftError("Unknown image/video format", __func__); }
    }
    else iftError("No original image path was given", "readImgInputs");

    iftVerifyImageDomains(*img, *label_img, "readImgInputs");
  } else (*img) = NULL;

  if(iftExistArg(args, "gt") == true)
  {
    if(iftHasArgVal(args, "gt") == true) 
    {
      PATH = iftGetArg(args, "gt");

      if(iftIsImageFile(PATH) == true) 
      { (*gt_img) = iftReadImageByExt(PATH);}
      else if(iftDirExists(PATH) == true)
      { (*gt_img) = iftReadImageFolderAsVolume(PATH);}
      else { iftError("Unknown image/video format", __func__); }
    }
    else iftError("No ground-truth image path was given", "readImgInputs");
    
    iftVerifyImageDomains(*gt_img, *label_img, "readImgInputs");
  } else (*gt_img) = NULL;

}
