Shader "Client Art/World Minimap"
{
    Properties
    {
        [PerRendererData] _MainTex ("Terrain atlas", 2D) = "white" {}
        _Color ("Tint", Color) = (1,1,1,1)
        _StencilComp ("Stencil Comparison", Float) = 8
        _Stencil ("Stencil ID", Float) = 0
        _StencilOp ("Stencil Operation", Float) = 0
        _StencilWriteMask ("Stencil Write Mask", Float) = 255
        _StencilReadMask ("Stencil Read Mask", Float) = 255
        _ColorMask ("Color Mask", Float) = 15
        [Toggle(UNITY_UI_ALPHACLIP)] _UseUIAlphaClip ("Use Alpha Clip", Float) = 0
    }
    SubShader
    {
        Tags { "Queue"="Transparent" "RenderType"="Transparent" "IgnoreProjector"="True" "CanUseSpriteAtlas"="False" }
        Stencil
        {
            Ref [_Stencil]
            Comp [_StencilComp]
            Pass [_StencilOp]
            ReadMask [_StencilReadMask]
            WriteMask [_StencilWriteMask]
        }
        Cull Off
        Lighting Off
        ZWrite Off
        ZTest [unity_GUIZTestMode]
        Blend SrcAlpha OneMinusSrcAlpha
        ColorMask [_ColorMask]
        Pass
        {
            CGPROGRAM
            #pragma vertex Vert
            #pragma fragment Frag
            #pragma target 2.0
            #pragma multi_compile_local _ UNITY_UI_CLIP_RECT
            #pragma multi_compile_local _ UNITY_UI_ALPHACLIP
            #include "UnityCG.cginc"
            #include "UnityUI.cginc"
            struct Attributes { float4 vertex : POSITION; fixed4 color : COLOR; float4 uv : TEXCOORD0; };
            struct Varyings { float4 vertex : SV_POSITION; fixed4 color : COLOR; float4 uv : TEXCOORD0; float2 localPosition : TEXCOORD1; };
            sampler2D _MainTex;
            fixed4 _Color;
            float4 _ClipRect;
            Varyings Vert(Attributes input)
            {
                Varyings output;
                output.vertex = UnityObjectToClipPos(input.vertex);
                output.localPosition = input.vertex.xy;
                output.color = input.color * _Color;
                output.uv = input.uv;
                return output;
            }
            fixed4 Frag(Varyings input) : SV_Target
            {
                // Negative UV belongs to overlay geometry, which must not inherit atlas colours.
                fixed4 colour = input.color;
                if (input.uv.x >= 0 && input.uv.y >= 0)
                {
                    fixed4 terrain = tex2D(_MainTex, input.uv.xy);
                    fixed luminance = dot(terrain.rgb, fixed3(.2126, .7152, .0722));
                    // Match the selected cool map face while preserving terrain detail and overlay colours.
                    terrain.rgb = lerp(luminance.xxx, terrain.rgb, .22) * fixed3(.62, .9, 1.18);
                    colour.rgb *= lerp(fixed3(.0745, .1882, .2235), terrain.rgb, .68);
                    colour.a *= terrain.a;
                }
                // Feather the whole map (terrain, range rings and markers) into the scene, without blurring its centre.
                float2 edge = min(input.uv.zw, 1 - input.uv.zw);
                float2 feather = smoothstep(0, .14, edge);
                colour.a *= feather.x * feather.y;
                #ifdef UNITY_UI_CLIP_RECT
                colour.a *= UnityGet2DClipping(input.localPosition, _ClipRect);
                #endif
                #ifdef UNITY_UI_ALPHACLIP
                clip(colour.a - .001);
                #endif
                return colour;
            }
            ENDCG
        }
    }
}
