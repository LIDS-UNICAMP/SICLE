/*****************************************************************************\
* RunRelabel.c
*
* AUTHOR  : Felipe Belem
* DATE    : 2023-06-09
* LICENSE : MIT License
* EMAIL   : felipe.belem@ic.unicamp.br
\*****************************************************************************/
#include "ift.h"
#include "iftArgs.h"
#include "iftMetrics.h"

/* HEADERS __________________________________________________________________*/
void usage();
void readImgInputs
(iftArgs *args, iftImage **labels, const char **path, bool *is_video);

/* MAIN _____________________________________________________________________*/
int main(int argc, char const *argv[])
{
  /*-------------------------------------------------------------------------*/
  bool has_req, has_help;
  iftArgs *args;

  args = iftCreateArgs(argc, argv);
  has_req = iftExistArg(args, "labels") && iftExistArg(args, "out");
  has_help = iftExistArg(args, "help");

  if(!has_req || has_help)
  { usage(); iftDestroyArgs(&args); return EXIT_FAILURE; }
  /*-------------------------------------------------------------------------*/
  const char *OUT_PATH;
  bool is_video;
  iftImage *img, *label_img, *out_img;

  readImgInputs(args, &label_img, &OUT_PATH, &is_video);
  iftDestroyArgs(&args);

  out_img = iftRelabelImage(label_img);
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
  printf("%-*s %s\n", SKIP_IND, "--labels", 
         "Input label");
  printf("%-*s %s\n", SKIP_IND, "--out", 
         "Output relabeled colored image");

  printf("\nThe optional parameters are:\n");
  printf("%-*s %s\n", SKIP_IND, "--help", 
         "Prints this message");

  printf("\n");
}

void readImgInputs
(iftArgs *args, iftImage **labels, const char **path, bool *is_video)
{
  #if IFT_DEBUG /*###########################################################*/
  assert(args != NULL);
  assert(img != NULL);
  assert(labels != NULL);
  assert(path != NULL);
  #endif /*##################################################################*/
  const char *PATH;

  if(iftHasArgVal(args, "labels")) 
  {
    PATH = iftGetArg(args, "labels");

    if(iftIsImageFile(PATH)) 
    { (*labels) = iftReadImageByExt(PATH); (*is_video) = false;}
    else if(iftDirExists(PATH))
    { (*labels) = iftReadImageFolderAsVolume(PATH); (*is_video) = true;}
    else { iftError("Unknown image/video format", __func__); }
  }
  else iftError("No label image path was given", __func__);

  if(iftHasArgVal(args, "out") == true)
  { (*path) = iftGetArg(args, "out"); }
  else iftError("No output image path was given", __func__);
}