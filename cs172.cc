#include <iostream>
#include <cmath>

using namespace std;

int main()
{
    double w = 1;
    double x = 1;
    double y = 1;
    double z = 1;

    float d = 0.85;
    int N = 4;
    float init = 0;
    int counter = 0;
    float sigma = 0.001;


    double compW = 1;
    double compX = 1;
    double compY = 1;
    double compZ = 1;

    double prW = 1;
    double prW_temp = 0;
    double prX = 1;
    double prX_temp = 0;
    double prY = 1;
    double prY_temp = 0;
    double prZ = 1;
    double prZ_temp = 0;

    init = (1-d)/N;
    while((compW > sigma) || (compX > sigma) || (compY > sigma) || (compZ > sigma) )
    {
        prW_temp = prW;
        prX_temp = prX;
        prY_temp = prY;
        prZ_temp = prZ;
        prW = init + d*((prY)/1);
        prX = init + d*((prW)/2);
        prY = init + d*((prX)/1 + (prW)/2);
        prZ = init;
        counter++;
        compW = prW - prW_temp;
        compX = prX - prX_temp;
        compY = prY - prY_temp;
        compZ = prZ - prZ_temp;
        if(compX < 0)
        {
            compX *= -1;
        }
        if(compY < 0)
        {
            compY *= -1;
        }
        if(compZ < 0)
        {
            compZ *= -1;
        }
        if(compW < 0)
        {
            compW *= -1;
        }
    }
    cout << "Page rank of page 0 is: " << prW << endl;
    cout << "Page rank of page 1 is: " << prX << endl;
    cout << "Page rank of page 2 is: " << prY << endl;
    cout << "Page rank of page 3 is: " << prZ << endl;
    cout << "The count is: " << counter << endl;
}
