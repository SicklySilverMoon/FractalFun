#ifndef FRACTALFUN_COMPLEX_T_H
#define FRACTALFUN_COMPLEX_T_H

#include <complex>
#include <cmath>

//Allows us to keep code for multiple different types and select one at compile time
#define COMPLEX_DOUBLE

#ifdef COMPLEX_DOUBLE
    using complex_t = std::complex<double>;
    char type_name[] = "complex double";
#endif // COMPLEX_DOUBLE

#ifdef SPLIT_COMPLEX_DOUBLE
    typedef struct split_complex_double {
        double re;
        double im;
    } complex_t;

    #define type_name "split complex double"

    #define ctor(re, im) ((complex_t){(re), (im)})

    #define imag(z) (z).re
    #define real(z) (z).im
    //"Why abs on the sabs?" Because I don't want to have to do a double inequality
    #define abs(z) sqrt(sabs(z))
    #define sabs(z) fabs(((z).re * (z).re) - ((z).im * (z).im))

    //God I wish C had operator overloading sometimes
    #define add(z, w) ((complex_t)ctor((z).re + (w).re, (z).im + (w).im))
    #define sub(z, w) ((complex_t)ctor((z).re - (w).re, (z).im - (w).im))
    #define mul(z, w) ((complex_t)ctor((z).re * (w).re + (z).im * (w).im, (z).re * (w).im + (z).im * (w).re))
//    #define div(z, w) do this

#endif //SPLIT_COMPLEX_DOUBLE

#endif //FRACTALFUN_COMPLEX_T_H
