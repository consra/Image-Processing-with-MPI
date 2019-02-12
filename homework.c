#include<mpi.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MSG_LEN               20
#define HEADER_LEN            10
#define TYPE_LEN              2

int resize_factor;
int num_threads;

typedef unsigned char uchar;

typedef struct {
	uchar R;
	uchar G;
	uchar B;
} pixel;

typedef struct {
	int width;
	int height;
	int maxValue;
	pixel *pixels;
	uchar *pixels_g;
	uchar type[3];
} image;

/* Smoothing filter */
float SF[3][3] = {
	{1.f/9, 1.f/9, 1.f/9},
	{1.f/9, 1.f/9, 1.f/9},
	{1.f/9, 1.f/9, 1.f/9}
};

/* Gaussian Blur Filter */
float BF[3][3] = {
	{1.f/16, 2.f/16, 1.f/16},
	{2.f/16, 4.f/16, 2.f/16},
	{1.f/16, 2.f/16, 1.f/16}
};

/* Sharpen */
float SH[3][3] = {
	{     0, -2.f/3,      0},
	{-2.f/3,   11.f/3, -2.f/3},
	{     0, -2.f/3,      0}
};

/* Mean removal */
float MR[3][3] = {
	{-1.f, -1.f,  -1.f},
	{-1.f,  9.f,  -1.f},
	{-1.f, -1.f,  -1.f}
};

/* Emboss */
float EM[3][3] = {
	{0.f,  1.f, 0.f},
	{0.f,  0.f, 0.f},
	{0.f, -1.f, 0.f}
};

int get_type(image *img) {
	if(memcmp(img->type, "P5", TYPE_LEN) == 0) {
		return 0;
	} else {
		return 1;
	}
}

pixel get_even(image *in, int starti, int startj, int filter, int ret) {
	pixel new_pixel;
	new_pixel.R = 0;
	new_pixel.G = 0;
	new_pixel.B = 0;
	float sum_R = 0, sum_G = 0, sum_B = 0;

	if(ret == 1) {
		for(int i = starti, k = 0; i < starti + 3 && k < 3; i++, k++) {
			for(int j = startj, r = 0; j < startj + 3 && r < 3; j++, r++) {
				if(filter == 1)
				{
					sum_R += (float)in->pixels[i * in->width + j].R * SF[k][r];
					sum_G += (float)in->pixels[i * in->width + j].G * SF[k][r];
					sum_B += (float)in->pixels[i * in->width + j].B * SF[k][r];
				}
				if(filter == 2)
				{
					sum_R += (float)in->pixels[i * in->width + j].R * BF[k][r];
					sum_G += (float)in->pixels[i * in->width + j].G * BF[k][r];
					sum_B += (float)in->pixels[i * in->width + j].B * BF[k][r];
				}
				if(filter == 3)
				{
					sum_R += (float)in->pixels[i * in->width + j].R * SH[k][r];
					sum_G += (float)in->pixels[i * in->width + j].G * SH[k][r];
					sum_B += (float)in->pixels[i * in->width + j].B * SH[k][r];
				}
				if(filter == 4)
				{
					sum_R += (float)in->pixels[i * in->width + j].R * MR[k][r];
					sum_G += (float)in->pixels[i * in->width + j].G * MR[k][r];
					sum_B += (float)in->pixels[i * in->width + j].B * MR[k][r];
				}
				if(filter == 5)
				{
					sum_R += (float)in->pixels[i * in->width + j].R * EM[k][r];
					sum_G += (float)in->pixels[i * in->width + j].G * EM[k][r];
					sum_B += (float)in->pixels[i * in->width + j].B * EM[k][r];
				}
			}
		}

		new_pixel.R = (uchar)(sum_R);
		new_pixel.G = (uchar)(sum_G);
		new_pixel.B = (uchar)(sum_B);

	} else {
		for(int i = starti, k = 0; i < starti + 3 && k < 3; i++, k++) {
			for(int j = startj, r = 0; j < startj + 3 && r < 3; j++, r++) {
				if(filter == 1)
					sum_R += in->pixels_g[i * in->width + j] * SF[k][r];
				else if(filter == 2)
					sum_R += in->pixels_g[i * in->width + j] * BF[k][r];
				else if(filter == 3)
					sum_R += in->pixels_g[i * in->width + j] * SH[k][r];
				else if(filter == 4)
					sum_R += in->pixels_g[i * in->width + j] * MR[k][r];
				else if(filter == 5)
					sum_R += in->pixels_g[i * in->width + j] * EM[k][r];
			}
		}

		new_pixel.R = (uchar)(sum_R);
	}

	return new_pixel;
}

void readInput(const char * fileName, image *img) {

	/* we first open the file */
	FILE *input_file = fopen(fileName, "rb");
	if(input_file == 0) {
		puts("The file couldn't be opened");
	}

	/* reads the header of the file to see the format */
	char file_header[HEADER_LEN];
	fgets(file_header, HEADER_LEN, input_file);

	/* sets the type of the image */
	memcpy(img->type, file_header, TYPE_LEN);
	img->type[2] = '\0';
	if(memcmp(img->type, "P5", TYPE_LEN) != 0 &&
	   memcmp(img->type, "P6", TYPE_LEN) != 0) {
        puts("The header type is wrong");
	}

	/* reads with and height of the image */
	if(fscanf(input_file, "%d %d\n", &(img->width), &(img->height)) != 2) {
		puts("The height and the width were not red corectly");
	}
	/* initialize the matrix of pixels */
	if(get_type(img) == 1)
		img->pixels = (pixel*) malloc(sizeof(pixel*) * img->height  * img->width);
	else
		img->pixels_g = (uchar*) malloc(sizeof(char*) * img->height  * img->width);

	/* reads maxValue */
	if(fscanf(input_file, "%d\n", &(img->maxValue)) != 1) {
		puts("The height and the width were not red corectly");
	}

	/* reading the image content */
	if(memcmp(img->type, "P5", TYPE_LEN) == 0) {
		fread(img->pixels_g, img->width, img->height, input_file);
	} else {
		fread(img->pixels, img->width * 3, img->height, input_file);
    }

   fclose(input_file);
}

void writeData(const char * fileName, image *img) {

	FILE *output_file = fopen(fileName, "wb+");
	if(output_file == 0) {
		puts("The file couldn't be opened");
	}

	/* wrinting the type of the image */
	fprintf(output_file, "%s\n", img->type);

	/* writing the width and the height of the image */
	fprintf(output_file, "%d %d\n", img->width, img->height);

	/* writng the maxValue*/
	fprintf(output_file, "%d\n", img->maxValue);

	/* write data back to file */
	if(memcmp(img->type, "P5", TYPE_LEN) == 0) {
		fwrite(img->pixels_g, img->width, img->height, output_file);
		free(img->pixels_g);
	} else {
		fwrite(img->pixels, img->width * 3, img->height, output_file);
		free(img->pixels);
    }

	fclose(output_file);
}

int get_filter_type(char filter[])
{
	if(strcmp("smooth", filter) == 0)
		return 1;
	if(strcmp("blur", filter) == 0)
		return 2;
	if(strcmp("sharpen", filter) == 0)
		return 3;
	if(strcmp("mean", filter) == 0)
		return 4;
	if(strcmp("emboss", filter) == 0)
		return 5;

	return 0;
}

int main(int argc, char *argv[]) {

	int rank;
	int nProcesses;
	MPI_Init(&argc, &argv);
	MPI_Comm_rank(MPI_COMM_WORLD, &rank);
	MPI_Comm_size(MPI_COMM_WORLD, &nProcesses);
	image in;
	image out;
	int ret;
	int height;
	int width;
	if(nProcesses == 1)
	{
		readInput(argv[1], &in);

		out.maxValue = in.maxValue;
		out.width = in.width;
		out.height = in.height;

		int ret = get_type(&in);
		if(ret == 1)
			out.pixels = (pixel*) malloc(sizeof(pixel) * out.height * out.width);
		else
			out.pixels_g = (uchar*) malloc(sizeof(uchar) * out.height * out.width);
		memcpy(out.type, in.type, TYPE_LEN+1);

		for(int i = 3; i < argc; i++) {
			int type = get_filter_type(argv[i]);
			for(int i = 0; i < in.height; i++) {
				for(int j = 0; j < in.width; j++) {
					if(ret == 1)
					{
						if(i == 0 || j == 0 || i == in.height - 1 || j == in.width - 1)
							out.pixels[i * out.width + j] = in.pixels[i * out.width + j];
						if( i < in.height - 1 && j < in.width - 1)
							out.pixels[(i+1) * out.width + (j+1)] = get_even(&in, i, j, type, ret);
					}
					else
					{
						if(i == 0 || j == 0 || i == in.height - 1 || j == in.width - 1)
							out.pixels_g[i * out.width + j] = in.pixels_g[i * out.width + j];
						if( i < in.height - 1 && j < in.width - 1)
							out.pixels_g[(i+1) * out.width + (j+1)] = (get_even(&in, i, j, type, ret)).R;
					}
				}
			}
			if(ret == 1)
				memcpy(in.pixels, out.pixels, sizeof(pixel) * out.height * out.width);
			else
				memcpy(in.pixels_g, out.pixels_g, sizeof(uchar) * out.height * out.width);
		}

		writeData(argv[2], &out);
	}
	else
	{
		if(rank == 0)
		{
			/* read Input */
			readInput(argv[1], &in);
			out.maxValue = in.maxValue;
			memcpy(out.type, in.type, TYPE_LEN+1);
			out.width = in.width;
			out.height = in.height;
			width = out.width;
			height = out.height;

			ret = get_type(&in);
			if(ret == 1)
				out.pixels = (pixel*) malloc(sizeof(pixel) * out.height * out.width);
			else
				out.pixels_g = (uchar*) malloc(sizeof(uchar) * out.height * out.width);

			for(int i = 1; i < nProcesses; i++)
			{
				MPI_Send(&ret, 1, MPI_INT, i, 0, MPI_COMM_WORLD);
				MPI_Send(&width, 1, MPI_INT, i, 0, MPI_COMM_WORLD);
				MPI_Send(&height, 1, MPI_INT, i, 0, MPI_COMM_WORLD);
			}
		}

		if(rank != 0)
		{
			MPI_Recv(&ret, 1, MPI_INT, 0, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
			MPI_Recv(&width, 1, MPI_INT, 0, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
			MPI_Recv(&height, 1, MPI_INT, 0, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
			if(ret == 1)
			{
				out.pixels = (pixel*) malloc(sizeof(pixel) * height * width);
				in.pixels = (pixel*) malloc(sizeof(pixel) * height * width);
			}
			else
			{
				out.pixels_g = (uchar*) malloc(sizeof(uchar) * height * width);
				in.pixels_g = (uchar*) malloc(sizeof(uchar) * height * width);
			}
		}

		for(int r = 3; r < argc; r++)
		{
			int type = get_filter_type(argv[r]);
			if(rank == 0)
			{
				for(int i = 1; i < nProcesses; i++)
					if(ret == 1)
						MPI_Send(in.pixels, 3 * width * height, MPI_CHAR, i, 0,MPI_COMM_WORLD);
					else
						MPI_Send(in.pixels_g, width * height, MPI_CHAR, i, 0, MPI_COMM_WORLD);
			}

			if(rank != 0)
			{
				if(ret == 1)
					MPI_Recv(in.pixels, 3 * width * height, MPI_CHAR, 0, 0,MPI_COMM_WORLD, MPI_STATUS_IGNORE);
				else
					MPI_Recv(in.pixels_g, width * height, MPI_CHAR, 0, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

				int start = (rank - 1) * (height / (3 * (nProcesses - 1))) * 3;
				if(start > 0)
					start = start - 1;
				in.width = width;
				in.height = height;
				int end = rank * (height / (3 * (nProcesses - 1))) * 3;
				if(rank == nProcesses -1)
					end = height;
				for(int i = start; i < end; i++) {
					for(int j = 0; j < width; j++) {
						if(ret == 1)
						{
							if(i == 0 || j == 0 || i == height - 1 || j == width - 1)
								out.pixels[i * width + j] = in.pixels[i * width + j];
							if( i < height - 1 && j < width - 1)
								out.pixels[(i+1) * width + (j+1)] = get_even(&in, i, j, type, ret);
						}
						else
						{
							if(i == 0 || j == 0 || i == height - 1 || j == width - 1)
								out.pixels_g[i * width + j] = in.pixels_g[i * width + j];
							if( i < height - 1 && j < width - 1)
								out.pixels_g[(i+1) * width + (j+1)] = (get_even(&in, i, j, type, ret)).R;
						}
					}
				}
				if(start > 0)
					start = start + 1;

				if(ret == 1)
				{
					for(int i = start; i < end; i++)
						MPI_Send(out.pixels + (i * width), 3 * width, MPI_CHAR, 0 , 0, MPI_COMM_WORLD);
				}
				else
				{
					for(int i = start; i < end; i++)
						MPI_Send(out.pixels_g + (i * width), width, MPI_CHAR, 0 , 0, MPI_COMM_WORLD);
				}
			}
			else
			{
				if(rank == 0)
				{
					if(ret == 1)
						for(int i = 1; i < nProcesses; i++)
						{
							int start = (i - 1) * (height / (3 * (nProcesses - 1))) * 3;
							int end = i * (height / (3 * (nProcesses - 1))) * 3;
							if(i == nProcesses - 1)
								end = height;
							for(int j = start; j < end; j++)
								MPI_Recv(out.pixels + j * width, 3 * width, MPI_CHAR, i, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
						}
					else
						for(int i = 1; i < nProcesses; i++)
						{
							int start = (i - 1) * (height / (3 * (nProcesses - 1))) * 3;
							int end = i * (height / (3 * (nProcesses - 1))) * 3;
							if(i == nProcesses -1)
								end = height;
							for(int j = start; j < end; j++)
								MPI_Recv(out.pixels_g + j * width, width, MPI_CHAR, i, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
						}
					if(ret == 1)
						memcpy(in.pixels, out.pixels, sizeof(pixel) * out.height * out.width);
					else
						memcpy(in.pixels_g, out.pixels_g, sizeof(uchar) * out.height * out.width);
				}
			}
		}
		if(rank == 0)
		{
			writeData(argv[2], &out);
		}
	}
	MPI_Finalize();
	return 0;
}
