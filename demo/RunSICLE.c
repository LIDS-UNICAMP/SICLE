/*****************************************************************************\
* RunSICLE.c
*
* AUTHOR  : Felipe Belem
* DATE    : 2022-06-15
* LICENSE : MIT License
* EMAIL   : felipe.belem@ic.unicamp.br
\*****************************************************************************/
#include "ift.h"
#include "iftArgs.h"
#include "iftSICLE.h"

/* PROTOTYPES ****************************************************************/
void readImgInputs
(iftArgs *args, iftImage **img, iftImage **mask, iftImage **objsm, 
	const char **path, bool *is_video);

void readSICLEArgs
(iftArgs *args, iftSICLEArgs **sargs);

void usage();

char *remove_ext (const char* myStr, char extSep, char pathSep);

/* MAIN **********************************************************************/
int main(int argc, char const *argv[])
{
	//-----------------------------------------------------------------------//
	bool has_req, has_help;
	iftArgs *args;

	args = iftCreateArgs(argc, argv);

	has_req = iftExistArg(args, "img")  && iftExistArg(args, "out");
	has_help = iftExistArg(args, "help");

	if(!has_req || has_help)
	{ usage(); iftDestroyArgs(&args); return EXIT_FAILURE; }
	//-----------------------------------------------------------------------//
	bool multiscale, is_video;
	const char* OUT;
	iftSICLEArgs *sargs;
	iftSICLE *sicle;
	iftImage *img, *objsm, *mask;

	multiscale = iftExistArg(args, "multiscale");
	readImgInputs(args, &img, &mask, &objsm, &OUT, &is_video);
	readSICLEArgs(args, &sargs);
	iftDestroyArgs(&args);
	
	sicle = iftCreateSICLE(img, objsm, mask);
	iftDestroyImage(&img);
	if(objsm != NULL) { iftDestroyImage(&objsm); }
	if(mask != NULL) { iftDestroyImage(&mask); }

	iftVerifySICLEArgs(sicle, sargs);
	if(multiscale == false)
	{
		iftImage *segm;

		segm = iftRunSICLE(sicle, sargs);
		if(is_video == false)
		{ iftWriteImageByExt(segm, OUT); }
		else
		{ iftWriteVolumeAsSingleVideoFolder(segm, OUT); }
		iftDestroyImage(&segm);
	}
	else
	{
		const char *EXT;
		int num_scales;
		char *basename;
		iftImage **multisegm;

	  EXT = iftFileExt(OUT);
	  basename = remove_ext(OUT,'.','/');
		multisegm = iftRunMultiscaleSICLE(sicle, sargs, &num_scales);
		for(int i = 0; i < num_scales; ++i)
		{
			if(is_video == false)
			{ iftWriteImageByExt(multisegm[i], "%s_%d%s", basename, i+1, EXT); }
			else
			{ 
				char tmp[IFT_STR_DEFAULT_SIZE];

				sprintf(tmp, "%s_%d/%s",basename,i+1,EXT);
				iftWriteVolumeAsSingleVideoFolder(multisegm[i], tmp); 
			}
			iftDestroyImage(&(multisegm[i]));
		}
		free(multisegm);
	}
	iftDestroySICLE(&sicle);
	iftDestroySICLEArgs(&sargs);


	return EXIT_SUCCESS;
}
/* METHODS********************************************************************/
void readImgInputs
(iftArgs *args, iftImage **img, iftImage **mask, iftImage **objsm, 
	const char **path, bool *is_video)
{
	const char *VAL;
	if(iftHasArgVal(args, "img") == true)
	{
		VAL = iftGetArg(args,"img");
		if(iftIsImageFile(VAL) == true) 
		{ (*img) = iftReadImageByExt(VAL); (*is_video) = false; }
		else if(iftDirExists(VAL) == true)
		{ (*img) = iftReadImageFolderAsVolume(VAL); (*is_video) = true;}
		else { iftError("Unknown image/video format", __func__); } 
	}
	else { iftError("No image path was given", __func__); }

	if(iftHasArgVal(args, "out") == true)
	{ (*path) = iftGetArg(args, "out"); }
	else { iftError("No output path was given", __func__); }

	if(iftExistArg(args, "mask") == false) { (*mask) = NULL; }
	else if(iftHasArgVal(args, "mask") == true)
	{
		VAL = iftGetArg(args,"mask");
		if(iftIsImageFile(VAL) == true) 
		{ (*mask) = iftReadImageByExt(VAL); }
		else if(iftDirExists(VAL) == true)
		{ (*mask) = iftReadImageFolderAsVolume(VAL); }
		else { iftError("Unknown image/video format", __func__); }

		iftVerifyImageDomains((*img), (*mask), __func__);
 	}
	else { iftError("No mask path was given", __func__); }

	if(iftExistArg(args, "objsm") == false) { (*objsm) = NULL; }
	else if(iftHasArgVal(args, "objsm") == true)
	{
		VAL = iftGetArg(args,"objsm");
		if(iftIsImageFile(VAL) == true) 
		{ (*objsm) = iftReadImageByExt(VAL); }
		else if(iftDirExists(VAL) == true)
		{ (*objsm) = iftReadImageFolderAsVolume(VAL); } 
		else { iftError("Unknown image/video format", __func__); }

		iftVerifyImageDomains((*img), (*objsm), __func__);
	}
	else { iftError("No saliency map path was given", __func__); }
}

void readSICLEArgs
(iftArgs *args, iftSICLEArgs **sargs)
{
	(*sargs) = iftCreateSICLEArgs();

	(*sargs)->use_diag = !iftExistArg(args, "no-diag");
	(*sargs)->use_dift = !iftExistArg(args, "no-dift");

	if(iftExistArg(args, "max-iters") == true)
	{
		if(iftHasArgVal(args, "max-iters") == true) 
		{ (*sargs)->max_iters = atoi(iftGetArg(args, "max-iters")); }
		else { iftError("No maximum number of iterations was given", __func__); }
	}

	if(iftExistArg(args, "n0") == true)
	{
		if(iftHasArgVal(args, "n0") == true) 
		{ (*sargs)->n0 = atoi(iftGetArg(args, "n0")); }
		else { iftError("No initial quantity of seeds was given", __func__); }
	}

	if(iftExistArg(args, "nf") == true)
	{ 
		if(iftHasArgVal(args, "nf") == true) 
		{ (*sargs)->nf = atoi(iftGetArg(args, "nf")); }
		else { iftError("No final quantity of superspels was given", __func__); }
	}

	if(iftExistArg(args, "irreg") == true)
	{
		if(iftHasArgVal(args, "irreg") == true) 
		{ (*sargs)->irreg = atof(iftGetArg(args, "irreg")); }
		else { iftError("No compacity factor was given", __func__); }
	}

	if(iftExistArg(args, "adhr") == true)
	{
		if(iftHasArgVal(args, "adhr") == true) 
		{ (*sargs)->adhr = atoi(iftGetArg(args, "adhr")); }
		else { iftError("No boundary adherence factor was given", __func__); }
	}

	if(iftExistArg(args, "alpha") == true)
	{
		if(iftHasArgVal(args, "alpha") == true) 
		{ (*sargs)->alpha = atof(iftGetArg(args, "alpha")); }
		else { iftError("No alpha fator was given", __func__); }
	}

	if(iftExistArg(args, "sampl-opt") == true)
	{
		if(iftHasArgVal(args, "sampl-opt") == true)
		{
			const char *VAL = iftGetArg(args, "sampl-opt");

	    if(iftCompareStrings(VAL, "grid"))
	    { (*sargs)->samplopt = IFT_SICLE_SAMPL_GRID; }
		else if(iftCompareStrings(VAL, "rnd"))
	    { (*sargs)->samplopt = IFT_SICLE_SAMPL_RND; }
		else if(iftCompareStrings(VAL, "custom"))
	    { (*sargs)->samplopt = IFT_SICLE_SAMPL_CUSTOM; }
	    else iftError("Unknown seed oversampling option", __func__);
		}
		else { iftError("No seed oversampling option was given", __func__); }	
	}

	if(iftExistArg(args, "conn-opt") == true)
	{
		if(iftHasArgVal(args, "conn-opt") == true)
		{
			const char *VAL = iftGetArg(args, "conn-opt");

	    if(iftCompareStrings(VAL, "fmax"))
	    { (*sargs)->connopt = IFT_SICLE_CONN_FMAX; }
		else if(iftCompareStrings(VAL, "fsum"))
	    { (*sargs)->connopt = IFT_SICLE_CONN_FSUM; }
		else if(iftCompareStrings(VAL, "custom"))
	    { (*sargs)->connopt = IFT_SICLE_CONN_CUSTOM; }
	    else iftError("Unknown IFT connectivity function option", __func__);
		}
		else { iftError("No IFT connectivity function was given", __func__); }	
	}

	if(iftExistArg(args, "crit-opt") == true)
	{
		if(iftHasArgVal(args, "crit-opt") == true)
		{
			const char *VAL = iftGetArg(args, "crit-opt");

			if(iftCompareStrings(VAL, "size"))
	    { (*sargs)->critopt = IFT_SICLE_CRIT_SIZE; }
	    else if(iftCompareStrings(VAL, "minsc"))
	    { (*sargs)->critopt = IFT_SICLE_CRIT_MINSC; }
		  else if(iftCompareStrings(VAL, "maxsc"))
	    { (*sargs)->critopt = IFT_SICLE_CRIT_MAXSC; }
		  else if(iftCompareStrings(VAL, "spread"))
	    { (*sargs)->critopt = IFT_SICLE_CRIT_SPREAD; }
		  else if(iftCompareStrings(VAL, "custom"))
	    { (*sargs)->critopt = IFT_SICLE_CRIT_CUSTOM; }
	    else iftError("Unknown seed removal criterion", __func__);
		}
		else { iftError("No seed removal criterion was given", __func__); }	
	}

	if(iftExistArg(args, "pen-opt") == true)
	{
		if(iftHasArgVal(args, "pen-opt") == true)
		{
			const char *VAL = iftGetArg(args, "pen-opt");

	    if(iftCompareStrings(VAL, "obj"))
	    { (*sargs)->penopt = IFT_SICLE_PEN_OBJ; }
		  else if(iftCompareStrings(VAL, "bord"))
	    { (*sargs)->penopt = IFT_SICLE_PEN_BORD; }
		  else if(iftCompareStrings(VAL, "none"))
	    { (*sargs)->penopt = IFT_SICLE_PEN_NONE; }
		  else if(iftCompareStrings(VAL, "osb"))
	    { (*sargs)->penopt = IFT_SICLE_PEN_OSB; }
		  else if(iftCompareStrings(VAL, "bobs"))
	    { (*sargs)->penopt = IFT_SICLE_PEN_BOBS; }
		  else if(iftCompareStrings(VAL, "custom"))
	    { (*sargs)->penopt = IFT_SICLE_PEN_CUSTOM; }
	    else iftError("Unknown seed removal criterion", __func__);
		}
		else { iftError("No seed removal criterion was given", __func__); }	
	}
	
	if(iftExistArg(args, "ni") == true)
  {
    if(iftHasArgVal(args, "ni") == true)
    {
      const char *VAL;
      char *tmp, *tok;
      int i;
      iftSet *vals;

      vals = NULL;
      VAL = iftGetArg(args, "ni");
      tmp = iftCopyString(VAL);
      tok = strtok(tmp, ",");

      i = 0;
      while(tok != NULL)
      {
        iftInsertSet(&vals, atoi(tok));
        tok = strtok(NULL, ",");
        ++i;
     	}
      free(tmp);
      if(vals == NULL) { iftError("No list of Ni values was provided", __func__); }

      (*sargs)->user_ni = iftCreateIntArray(i);
      while(vals != NULL)
      { (*sargs)->user_ni->val[--i] = iftRemoveSet(&vals);}
    }
    else { iftError("No list of Ni values was provided", __func__); }
  }
  
}

void usage()
{
	const int SKIP_IND = 15; // For indentation purposes
	printf("\nMandatory parameters:\n");
	printf("%-*s %s\n", SKIP_IND, "--img",
		"Input image");
	printf("%-*s %s\n", SKIP_IND, "--out",
		"Output label image");

	printf("\nOptional files:\n");
	printf("%-*s %s\n", SKIP_IND, "--mask",
		"Mask image indicating the region of interest.");
	printf("%-*s %s\n", SKIP_IND, "--objsm",
		"Grayscale object saliency map.");

	printf("\nSICLE configuration options:\n");
	printf("%-*s %s\n", SKIP_IND, "--conn-opt",
		"IFT connectivity function. Options: "
		"fmax, fsum, custom. Default: fmax");
	printf("%-*s %s\n", SKIP_IND, "--crit-opt",
		"Seed removal criterion. Options: "
		"size, minsc, maxsc, spread, custom. Default: minsc");
	printf("%-*s %s\n", SKIP_IND, "--pen-opt",
		"Seed relevance penalization. Options: "
		"none, obj, bord, osb, bobs, custom. Default: none");

	printf("\nOptional general parameters:\n");
	printf("%-*s %s\n", SKIP_IND, "--multiscale",
		"Generates a multiscale segmentation.");
	printf("%-*s %s\n", SKIP_IND, "--no-diag",
		"Disable diagonal neighborhood (i.e., 8- or 26-adjacency).");
	printf("%-*s %s\n", SKIP_IND, "--no-dift",
		"Disable differential computation.");
	printf("%-*s %s\n", SKIP_IND, "--alpha",
		"Saliency information importance. Default: 0.0");
	printf("%-*s %s\n", SKIP_IND, "--irreg",
		"Superspel irregularity factor. Fsum only. Default: 0.12");
	printf("%-*s %s\n", SKIP_IND, "--adhr",
		"Superspel boundary adherence factor. Fsum only. Default: 12");
	printf("%-*s %s\n", SKIP_IND, "--max-iters",
		"Maximum number of iterations for segmentation. It is ignored when --ni"
		" is provided. Default: 7");
	printf("%-*s %s\n", SKIP_IND, "--n0",
		"Desired initial number of seeds. Default: 3000");
	printf("%-*s %s\n", SKIP_IND, "--nf",
		"Desired final number of superpixels. Default: 200");
	printf("%-*s %s\n", SKIP_IND, "--ni",
		"Comma-separated list of intermediary seed quantity.");
	printf("%-*s %s\n", SKIP_IND, "--help",
		"Prints this message");

	printf("\n");
}

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
