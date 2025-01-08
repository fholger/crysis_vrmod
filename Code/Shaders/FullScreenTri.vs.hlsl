//By CeeJay.dk
//License : CC0 - http://creativecommons.org/publicdomain/zero/1.0/
 
//Basic Buffer/Layout-less fullscreen triangle vertex shader
void main(in uint id : SV_VertexID, out float4 position : SV_Position, out float2 texcoord : TEXCOORD0)
{
        /*
        //See: https://web.archive.org/web/20140719063725/http://www.altdev.co/2011/08/08/interesting-vertex-shader-trick/
 
           1  
        ( 0, 2)
        [-1, 3]   [ 3, 3]
            .
            |`.
            |  `.  
            |    `.
            '------`
           0         2
        ( 0, 0)   ( 2, 0)
        [-1,-1]   [ 3,-1]
 
        ID=0 -> Pos=[-1,-1], Tex=(0,0)
        ID=1 -> Pos=[-1, 3], Tex=(0,2)
        ID=2 -> Pos=[ 3,-1], Tex=(2,0)
        */
 
        texcoord.x = (id == 2) ?  2.0 :  0.0;
        texcoord.y = (id == 1) ?  2.0 :  0.0;
 
        position = float4(texcoord * float2(2.0, -2.0) + float2(-1.0, 1.0), 1.0, 1.0);
}
