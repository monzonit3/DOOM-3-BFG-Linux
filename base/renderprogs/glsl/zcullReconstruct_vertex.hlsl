struct VS_IN {
float4 position : POSITION ; 
} ;
struct VS_OUT {
float4 position : POSITION ; 
} ;
void main (VS_IN vertex , out VS_OUT result )  {
result . position = vertex . position ; 
result . position . z = - 1.0 ; 
} 
