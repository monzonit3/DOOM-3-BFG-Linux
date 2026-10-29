struct PS_OUT {
float depth : DEPTH ; 
float4 color : COLOR ; 
} ;
void main (out PS_OUT result )  {
result . depth = 1 ; 
result . color = float4 ( 0.0 , 0.0 , 0.0 , 0.0 ) ; 
} 
