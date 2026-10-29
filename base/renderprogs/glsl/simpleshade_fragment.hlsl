static float dot3 (float3 a , float3 b )  {return dot ( a , b ) ; } 
static float dot3 (float3 a , float4 b )  {return dot ( a , b . xyz ) ; } 
static float dot3 (float4 a , float3 b )  {return dot ( a . xyz , b ) ; } 
static float dot3 (float4 a , float4 b )  {return dot ( a . xyz , b . xyz ) ; } 
struct PS_IN {
float4 position : WPOS ; 
float4 texcoord0 : TEXCOORD0_centroid ; 
} ;
struct PS_OUT {
float4 color : COLOR ; 
} ;
static float2 screenPosToTexcoord (float2 pos , float4 bias_scale )  {return ( pos * bias_scale . zw + bias_scale . xy ) ; } 
void main (PS_IN fragment , out PS_OUT result )  {
const float renderWidth = 1280.0 ; 
const float renderHeight = 720.0 ; 
const float4 positionToViewTexture = float4 ( 0.5 / renderWidth , 0.5 / renderHeight , 1.0 / renderWidth , 1.0 / renderHeight ) ; 

float interpolatedZOverW = ( 1.0 - ( fragment . texcoord0 . z / fragment . texcoord0 . w ) ) ; 

float3 pos ; 
pos . z = 1.0 / interpolatedZOverW ; 
pos . xy = pos . z * ( 2.0 * screenPosToTexcoord ( fragment . position . xy , positionToViewTexture ) - 1.0 ) ; 

float3 normal = normalize ( cross ( ddy ( pos ) , ddx ( pos ) ) ) ; 


float3 L = normalize ( float3 ( 1.0 , 1.0 , 0.0 ) - pos ) ; 

result . color . xyz = float3 ( dot3 ( normal , L ) * 0.75 ) ; 
result . color . w = 1.0 ; 
} 
