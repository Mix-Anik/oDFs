#ifndef __BG_LIB_MATH_H__
#define __BG_LIB_MATH_H__

/* Forward declarations of math functions (defined in math library) */
extern double sqrt(double x);
extern double log(double x);
extern double pow(double x, double y);

#ifdef Q3_VM
/* QVM math function implementations (only for Q3VM, not for DLL builds) */
static float sqrtf(float x) {
	return (float)sqrt((double)x);
}

static float logf(float x) {
	return (float)log((double)x);
}

static float powf(float x, float y) {
	return (float)pow((double)x, (double)y);
}
#endif

#endif
