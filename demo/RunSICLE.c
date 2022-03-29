/*****************************************************************************\
* RunSICLE.c
*
* AUTHOR  : Felipe Belem
* DATE    : 2021-07-08
* LICENSE : MIT License
* EMAIL   : felipe.belem@ic.unicamp.br
\*****************************************************************************/
#include "ift.h"
#include "iftArgs.h"
#include "iftSICLE.h"

void usage();
void readImgInputs
(const iftArgs *args, iftImage **img, iftImage **mask, iftImage **objsm, 
 const char **path);
void setSICLEParams
(iftSICLE **sicle, const iftArgs *args);
void setSICLESampl
(iftSICLE **sicle, const iftArgs *args);
void setSICLEArcCost
(iftSICLE **sicle, const iftArgs *args);
void setSICLERem
(iftSICLE **sicle, const iftArgs *args);
void writeLabels
(const iftImage **labels, const char *path);

// from: https://stackoverflow.com/questions/2736753/how-to-remove-extension-from-file-name
char *remove_ext (const char* myStr, char extSep, char pathSep) {
    char *retStr, *lastExt, *lastPath;

    // Error checks and allocate string.

    if (myStr == NULL) return NULL;
    if ((retStr = malloc (strlen (myStr) + 1)) == NULL) return NULL;

    // Make a copy and find the relevant characters.

    strcpy (retStr, myStr);
    lastExt = strrchr (retStr, extSep);
    lastPath = (pathSep == 0) ? NULL : strrchr (retStr, pathSep);

    // If it has an extension separator.

    if (lastExt != NULL) {
        // and it's to the right of the path separator.

        if (lastPath != NULL) {
            if (lastPath < lastExt) {
                // then remove it.

                *lastExt = '\0';
            }
        } else {
            // Has extension separator with no path separator.

            *lastExt = '\0';
        }
    }

    // Return the modified string.

    return retStr;
}


int main(int argc, char const *argv[])
{
  //-------------------------------------------------------------------------//
  bool has_req, has_help;
  iftArgs *args;

  args = iftCreateArgs(argc, argv);

  has_req = iftExistArg(args, "img")  && iftExistArg(args, "out");
  has_help = iftExistArg(args, "help");

  if(has_req == false || has_help == true)
  {
    usage(); 
    iftDestroyArgs(&args);
    return EXIT_FAILURE;
  }
  //-------------------------------------------------------------------------//
  const char *LABEL_PATH, *EXT;
  char *basename;
  int scale;
  iftImage *img, *mask, *objsm;
  iftImage **labels;
  iftSICLE *sicle;

  readImgInputs(args, &img, &mask, &objsm, &LABEL_PATH);

  sicle = iftCreateSICLE(img, mask, objsm);
  iftDestroyImage(&img);
  if(mask != NULL) iftDestroyImage(&mask);
  if(objsm != NULL) iftDestroyImage(&objsm);

  setSICLEParams(&sicle, args);
  setSICLESampl(&sicle, args);
  setSICLEArcCost(&sicle, args);
  setSICLERem(&sicle, args);
  iftDestroyArgs(&args);
  
  labels = iftRunSICLE(sicle);
  
  EXT = iftFileExt(LABEL_PATH);
  basename = remove_ext(LABEL_PATH,'.','/');
  for(scale = 1; scale <= iftSICLEGetNumScales(sicle); ++scale)
  {

    iftWriteImageByExt(labels[scale], "%s_%d%s", basename, scale,EXT);
    iftDestroyImage(&(labels[scale]));
  }
  free(basename);

  iftWriteImageByExt(labels[0], LABEL_PATH);
  iftDestroyImage(&(labels[0]));
  free(labels);

  iftDestroySICLE(&sicle);

  return EXIT_SUCCESS;
}

void usage()
{
  const int SKIP_IND = 15; // For indentation purposes
  printf("\nThe required parameters are:\n");
  printf("%-*s %s\n", SKIP_IND, "--img", 
         "Input image");
  printf("%-*s %s\n", SKIP_IND, "--out", 
         "Output label image");

  printf("\nThe optional parameters are:\n");
  printf("%-*s %s\n", SKIP_IND, "--no-diag-adj", 
         "Disable search scope to consider 8-adjacency.");
  printf("%-*s %s\n", SKIP_IND, "--mask", 
         "Mask image indicating the region of interest.");
  printf("%-*s %s\n", SKIP_IND, "--max-iters", 
         "Maximum number of iterations for segmentation. Default: 5");
  printf("%-*s %s\n", SKIP_IND, "--n0", 
         "Desired initial number of seeds. Default: 3000");
  printf("%-*s %s\n", SKIP_IND, "--nf", 
         "Desired final number of superpixels. Default: 200");
  printf("%-*s %s\n", SKIP_IND, "--scales", 
         "Comma-separated list of superpixel scales. Default: None");
  printf("%-*s %s\n", SKIP_IND, "--objsm", 
         "Grayscale object saliency map.");
  printf("%-*s %s\n", SKIP_IND, "--help", 
         "Prints this message");

  printf("\nThe SICLE configuration options are:\n");
  printf("%-*s %s\n", SKIP_IND, "--arc-op", 
         "IFT arc cost function. Options: "
         "root, dyn. Default: root");
  printf("%-*s %s\n", SKIP_IND, "--sampl-op", 
         "Seed sampling algorithm. Options: "
         "grid, rnd. Default: rnd");
  printf("%-*s %s\n", SKIP_IND, "--rem-op", 
         "Seed removal criterion. Options: "
         "max-contr, min-contr, size, rnd, "
         "max-sc, min-sc. Default: min-sc");

  printf("\n");
}

void readImgInputs
(const iftArgs *args, iftImage **img, iftImage **mask, iftImage **objsm, 
 const char **path)
{
  #if IFT_DEBUG //-----------------------------------------------------------//
  assert(args != NULL);
  assert(img != NULL);
  assert(mask != NULL);
  assert(objsm != NULL);
  assert(path != NULL);
  #endif //------------------------------------------------------------------//
  const char *PATH;

  if(iftHasArgVal(args, "img") == true) 
  {
    PATH = iftGetArg(args, "img");

    (*img) = iftReadImageByExt(PATH);
  }
  else iftError("No image path was given", "readImgInputs");

  if(iftHasArgVal(args, "out") == true)
  {
    PATH = iftGetArg(args, "out");

    if(iftIsImagePathnameValid(PATH) == true) (*path) = PATH;
    else iftError("Unknown image type", "readImgInputs");
  }
  else iftError("No output label path was given", "readImgInputs");

  if(iftExistArg(args, "mask") == true)
  {
    if(iftHasArgVal(args, "mask") == true) 
    {
      PATH = iftGetArg(args, "mask");

      (*mask) = iftReadImageByExt(PATH);

      iftVerifyImageDomains(*img, *mask, "readImgInputs");
    }
    else iftError("No mask path was given", "readImgInputs");
  }
  else (*mask) = NULL;

  if(iftExistArg(args, "objsm") == true)
  {
    if(iftHasArgVal(args, "objsm") == true) 
    {
      PATH = iftGetArg(args, "objsm");

      (*objsm) = iftReadImageByExt(PATH);

      iftVerifyImageDomains(*img, *objsm, "readImgInputs");
    }
    else iftError("No object saliency map path was given", "readImgInputs");
  }
  else (*objsm) = NULL;
}

void setSICLEParams
(iftSICLE **sicle, const iftArgs *args)
{
  #if IFT_DEBUG //-----------------------------------------------------------//
  assert(sicle != NULL);
  assert(*sicle != NULL);
  assert(args != NULL);
  #endif //------------------------------------------------------------------//
  int n0, nf, max_iters;

  iftSICLEUseDiagAdj(sicle, !iftExistArg(args, "no-diag-adj"));

  if(iftExistArg(args, "max-iters") == true)
  {
    if(iftHasArgVal(args, "max-iters") == true) 
    {
      max_iters = atoi(iftGetArg(args, "max-iters"));
      if(max_iters <= 1) 
        iftError("The maximum number of iterations must be greater than 1",
                 "setSICLEParams");
      iftSICLESetMaxIters(sicle, max_iters);
    }
    else iftError("No maximum number of iterations was given", 
                  "setSICLEParams");
  }

  if(iftExistArg(args, "n0") == true)
  {
    if(iftHasArgVal(args, "n0") == true) 
    {
      n0 = atoi(iftGetArg(args, "n0"));
      iftSICLESetN0(sicle, n0);
    }
    else iftError("No initial number of seeds was given", "setSICLEParams");
  }
  
  if(iftExistArg(args, "nf") == true)
  {
    if(iftHasArgVal(args, "nf") == true) 
    {
      nf = atoi(iftGetArg(args, "nf"));
	  
	  if(nf < iftSICLEGetN0(*sicle)) iftSICLESetNf(sicle, nf);
      else iftError("The number of superpixels must be greater than N0", 
                    "setSICLEParams");
    }
    else iftError("No desired quantity of superpixels was given", 
                  "setSICLEParams");
  }

  if(iftExistArg(args, "scales") == true)
  {
    if(iftHasArgVal(args, "scales") == true) 
    {
      const char *VAL;
      int i, prev;
      char *tmp, *tok;
	  int num_scales;
	  int *scales;
	  iftSet *list_scales;

	  list_scales = NULL;

      VAL = iftGetArg(args, "scales");
      tmp = iftCopyString(VAL);
      tok = strtok(tmp, ",");

      i = 0;
	  prev = iftSICLEGetN0(*sicle);
      while(tok != NULL)
      {
        int curr;

        curr = atoi(tok);
        if(curr < prev && curr > iftSICLEGetNf(*sicle))
        {
		  iftInsertSet(&list_scales,curr);
		  prev = curr;
        }
        else iftError("The order of scales must be statically decreasing and within ]N0,Nf[",
        		   	  "setSICLEParams");

        tok = strtok(NULL, ",");
        i++;
      }
      free(tmp);

      if(i == 0) iftError("No list of scales was given", "setSICLEParams");

	  num_scales = i;
	  scales = calloc(num_scales, sizeof(int));
	  while(i > 0)
		scales[--i] = iftRemoveSet(&list_scales);
	  
      iftSICLESetScales(sicle, num_scales, scales);
	  free(scales);
    }
    else iftError("No list of superpixel scales was given", 
                  "setSICLEParams");
  }
}

void setSICLESampl
(iftSICLE **sicle, const iftArgs *args)
{
  #if IFT_DEBUG //-----------------------------------------------------------//
  assert(sicle != NULL);
  assert(*sicle != NULL);
  assert(args != NULL);
  #endif //------------------------------------------------------------------//
  if(iftExistArg(args, "sampl-op") == true)
  {
    if(iftHasArgVal(args, "sampl-op") == true)
    {
      const char *OPT;

      OPT = iftGetArg(args, "sampl-op");

      if(iftCompareStrings(OPT, "grid")) 
        iftSICLESetSamplOpt(sicle, IFT_SICLE_SAMPL_GRID);
      else if(iftCompareStrings(OPT, "rnd")) 
        iftSICLESetSamplOpt(sicle, IFT_SICLE_SAMPL_RND);
      else iftError("Unknown seed sampling algorithm", "setSICLESampl");
    }
    else iftError("No sampling algorithm was given", "setSICLESampl");
  }
}

void setSICLEArcCost
(iftSICLE **sicle, const iftArgs *args)
{
  #if IFT_DEBUG //-----------------------------------------------------------//
  assert(sicle != NULL);
  assert(*sicle != NULL);
  assert(args != NULL);
  #endif //------------------------------------------------------------------//
  if(iftExistArg(args, "arc-op") == true)
  {
    if(iftHasArgVal(args, "arc-op") == true)
    {
      const char *OPT;

      OPT = iftGetArg(args, "arc-op");

      if(iftCompareStrings(OPT, "root")) 
        iftSICLESetArcCostOpt(sicle, IFT_SICLE_ARCCOST_ROOT);
      else if(iftCompareStrings(OPT, "dyn")) 
        iftSICLESetArcCostOpt(sicle, IFT_SICLE_ARCCOST_DYN);
      else iftError("Unknown arc-cost function", "setSICLEArcCost");
    }
    else iftError("No arc-cost function was given", "setSICLEArcCost");
  }
}

void setSICLERem
(iftSICLE **sicle, const iftArgs *args)
{
  #if IFT_DEBUG //-----------------------------------------------------------//
  assert(sicle != NULL);
  assert(*sicle != NULL);
  assert(args != NULL);
  #endif //------------------------------------------------------------------//
  if(iftExistArg(args, "rem-op") == true)
  {
    if(iftHasArgVal(args, "rem-op") == true)
    {
      const char *OPT;

      OPT = iftGetArg(args, "rem-op");

      if(iftCompareStrings(OPT, "min-contr")) 
        iftSICLESetRemOpt(sicle, IFT_SICLE_REM_MINCONTR);
      else if(iftCompareStrings(OPT, "max-contr")) 
        iftSICLESetRemOpt(sicle, IFT_SICLE_REM_MAXCONTR);
      else if(iftCompareStrings(OPT, "max-sc")) 
        iftSICLESetRemOpt(sicle, IFT_SICLE_REM_MAXSC);
      else if(iftCompareStrings(OPT, "min-sc")) 
        iftSICLESetRemOpt(sicle, IFT_SICLE_REM_MINSC);
      else if(iftCompareStrings(OPT, "size")) 
        iftSICLESetRemOpt(sicle, IFT_SICLE_REM_SIZE);
      else if(iftCompareStrings(OPT, "rnd")) 
        iftSICLESetRemOpt(sicle, IFT_SICLE_REM_RND);
      else iftError("Unknown seed removal criterion", "setSICLERem");
    }
    else iftError("No seed removal criterion was given", "setSICLERem");
  }
}
