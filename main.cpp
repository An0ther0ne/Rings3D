#include <windows.h>
#include <math.h>
#include <stdio.h>
#include <d3d8.h>
#pragma comment (lib, "d3d8.lib")
#include <d3dx8.h> 
#pragma comment (lib, "d3dx8.lib")
#include <MMSystem.h> //нужна только ф-я timeGetTime
#pragma comment (lib, "WinMM.Lib") 

#define APPNAME  "Grid 3D"
char	AppTitle[96];
#define _RELEASE_(p) { if(p) { (p)->Release(); (p)=NULL; };};
#define _DELETE_(p)  { if(p) { delete (p);     (p)=NULL; };};

LPDIRECT3D8             p_d3d          = NULL;
LPDIRECT3DDEVICE8       p_d3d_Device   = NULL;
LPDIRECT3DVERTEXBUFFER8 p_VertexBuffer = NULL;
LPDIRECT3DINDEXBUFFER8  p_IndexBuffer  = NULL;
LPDIRECT3DTEXTURE8		p_Texture_001  = NULL;

D3DPRESENT_PARAMETERS	d3dpp;
D3DDISPLAYMODE			d3ddm;

D3DLIGHT8 light;	//источник света
D3DTEXTUREFILTERTYPE CurrentFilter;//Фильтр текстур
D3DFILLMODE			 CurrentFillMd;//Спопоб отображения

#define D3DFVF_CUSTOMVERTEX ( D3DFVF_XYZ | D3DFVF_NORMAL | D3DFVF_TEX1)// + текстуры (цвет вершины D3DFVF_DIFFUSE = D3DCOLOR color)
struct CUSTOMVERTEX { float x, y, z, nx, ny, nz, tu, tv; };//с нормалями и двумя текстурными координатами
struct MYVERTEX {D3DXVECTOR3 pos,nrm; D3DXVECTOR2 tex;};//с нормалями и двумя текстурными координатами

#define FLAG_SIZE         64//размер стороны (к-во квадратов на одной стороне)
#define NUM_FLAG_VERTICES ((FLAG_SIZE+1)*(FLAG_SIZE+1))//к-во необходимых вершин
#define NUM_FLAG_INDICES  (FLAG_SIZE*FLAG_SIZE*6)//к-во необходимых индексов (один квадрат состоит из двух полигонов и каждый полигон имеет 3 вершины)

HWND hWnd;
HINSTANCE hTInst,hPInst;
int CmdShow;
bool fPlayMode=true;
bool fWiteMode=false;
bool fAlfaBlnd=false;
bool isFullScreen=false;

float a=35.0f;
#define vertA {  -a, 0.0f,     -a,	0.0f,	1.0f,	1.0f,	0.0f,	1.0f}
#define vertB {  -a, 0.0f,      a,	0.0f,	1.0f,	1.0f,	0.0f,	0.0f}
#define vertC {   a, 0.0f,     -a,	0.0f,	1.0f,	1.0f,	1.0f,	1.0f}
#define vertD {   a, 0.0f,      a,	0.0f,	1.0f,	1.0f,	1.0f,	0.0f}
#define vertS {0.0f, a*1.2f, 0.0f,	0.0f,	1.0f,	1.0f,	0.5f,	0.5f}

CUSTOMVERTEX g_Vertices[12] = {
	vertS,	vertB,	vertA,
	vertS,	vertD,	vertB,
	vertS,	vertC,	vertD,
	vertS,	vertA,	vertC,
};

CUSTOMVERTEX g_IndexedV[5] = {
	vertA, vertB, vertC, vertD, vertS,
};

WORD d_Indexses[12]={
	4,1,0,	4,3,1,	4,2,3,	4,0,2,
};

WORD	R1=20,//Радиус кольца - см. пример №5
		NA=16,//фактически NA*2 - к-во сегментов (хорд) из которых состоит кольцо
		DR=2; //разница между внешним и внутр. радиусом а также толщина
MYVERTEX* pRVertices;//Указатель на структуру вершин кольца
//-----------------------------------------------------------------------------
// Name: DestroyDirect3D8 ()
// Desc: 
//-----------------------------------------------------------------------------
void DestroyDirect3D8 (void){
	_RELEASE_ (p_Texture_001);
	_RELEASE_ (p_IndexBuffer);
	_RELEASE_ (p_VertexBuffer);
	_RELEASE_ (p_d3d_Device);
	_RELEASE_ (p_d3d);
};
//-----------------------------------------------------------------------------
// Name: NormalizeTriangles()
// Desc:
//-----------------------------------------------------------------------------
VOID NormalizeTriangles(int i, CUSTOMVERTEX vertexlelist[]){
 	for (int j=0;j<i;j+=3){
		D3DXVECTOR3 v1 = D3DXVECTOR3 (vertexlelist[j+1].x-vertexlelist[j].x, vertexlelist[j+1].y-vertexlelist[j].y, vertexlelist[j+1].z-vertexlelist[j].z);
		D3DXVECTOR3 v2 = D3DXVECTOR3 (vertexlelist[j+2].x-vertexlelist[j+1].x, vertexlelist[j+2].y-vertexlelist[j+1].y, vertexlelist[j+2].z-vertexlelist[j+1].z);
		D3DXVec3Cross(&v1,&v1,&v2);
		D3DXVec3Normalize(&v1,&v1);
		vertexlelist[j].nx=v1.x;
		vertexlelist[j].ny=v1.y;
		vertexlelist[j].nz=v1.z;
		vertexlelist[j+1].nx=v1.x;
		vertexlelist[j+1].ny=v1.y;
		vertexlelist[j+1].nz=v1.z;
		vertexlelist[j+2].nx=v1.x;
		vertexlelist[j+2].ny=v1.y;
		vertexlelist[j+2].nz=v1.z;
	};
};
//-----------------------------------------------------------------------------
// Name: TransGeometry5()
// Desc: Трансформирует неиндексированный массив по времени
//-----------------------------------------------------------------------------
HRESULT TransGeometry5(){
	float da=D3DX_PI/256;
	MYVERTEX* pVertices;
	if(FAILED(p_VertexBuffer->Lock (
		0,
		NA*8*6*sizeof(MYVERTEX), 
		(BYTE**)&pVertices,
		0))) return E_FAIL;
	WORD i;
	float t;
	for(i=0; i<48*NA; i++){
		t=pRVertices[i].pos.x;
		pRVertices[i].pos.x=t*cosf(da)-pRVertices[i].pos.y*sinf(da);
		pRVertices[i].pos.y=t*sinf(da)+pRVertices[i].pos.y*cosf(da);
		t=pRVertices[i].nrm.x;
		pRVertices[i].nrm.x=t*cosf(da)-pRVertices[i].nrm.y*sinf(da);
		pRVertices[i].nrm.y=t*sinf(da)+pRVertices[i].nrm.y*cosf(da);

		t=pRVertices[i+48*NA].pos.x;
		pRVertices[i+48*NA].pos.x=t*cosf(da)-pRVertices[i+48*NA].pos.z*sinf(da);
		pRVertices[i+48*NA].pos.z=t*sinf(da)+pRVertices[i+48*NA].pos.z*cosf(da);
		t=pRVertices[i+48*NA].nrm.x;
		pRVertices[i+48*NA].nrm.x=t*cosf(da)-pRVertices[i+48*NA].nrm.z*sinf(da);
		pRVertices[i+48*NA].nrm.z=t*sinf(da)+pRVertices[i+48*NA].nrm.z*cosf(da);

		t=pRVertices[i+96*NA].pos.y;
		pRVertices[i+96*NA].pos.y=t*cosf(da)-pRVertices[i+96*NA].pos.z*sinf(da);
		pRVertices[i+96*NA].pos.z=t*sinf(da)+pRVertices[i+96*NA].pos.z*cosf(da);
		t=pRVertices[i+96*NA].nrm.y;
		pRVertices[i+96*NA].nrm.y=t*cosf(da)-pRVertices[i+96*NA].nrm.z*sinf(da);
		pRVertices[i+96*NA].nrm.z=t*sinf(da)+pRVertices[i+96*NA].nrm.z*cosf(da);
	};
	memcpy (pVertices, pRVertices, sizeof(MYVERTEX)*3*NA*48);
	p_VertexBuffer->Unlock();
	return  S_OK;
};
//-----------------------------------------------------------------------------
// Name: SetVS5(int x,y; CUSTOMVERTEX VS)
// Desc: 
//-----------------------------------------------------------------------------
void SetVS5(int R, MYVERTEX* VS){
	float	da=D3DX_PI/(float)NA;
	WORD i=0;
	for (float a=0.0f;a<2*D3DX_PI;a+=da){
		VS[i].pos = D3DXVECTOR3((float)R*cosf(a),(float)-DR,(float)R*sinf(a));			//v1	p1
		VS[i].nrm = D3DXVECTOR3(-cosf(a),0.0f,-sinf(a));								//v1	p1
		VS[i].tex = D3DXVECTOR2((float)(i-1)/4,(float)(i-1)/4);							//v1	p1
		VS[i+1]=VS[i];																	//v2	p1
		VS[i+1].pos.y=(float)DR;														//v2	p1
		VS[i+1].tex = D3DXVECTOR2((float)i/4,(float)(i-1)/4);							//v2	p1
		VS[i+2].pos = D3DXVECTOR3((float)R*cosf(a+da),(float)-DR,(float)R*sinf(a+da));	//v3	p1
		VS[i+2].nrm = D3DXVECTOR3(-cosf(a+da),0.0f,-sinf(a+da));						//v3	p1
		VS[i+2].tex = D3DXVECTOR2((float)(i-1)/4,(float)i/4);							//v3	p1
		VS[i+3]=VS[i+2];																//v3	p2
		VS[i+4]=VS[i+2];																//v4	p2
		VS[i+4].pos.y=(float)DR;														//v4	p2
		VS[i+4].tex = D3DXVECTOR2((float)i/4,(float)i/4);								//v4	p2
		VS[i+5]=VS[i+1];																//v2	p2
//======
		VS[i+6].pos = D3DXVECTOR3((float)(R+DR)*cosf(a),(float)-DR,(float)(R+DR)*sinf(a));		//v5	p3
		VS[i+6].nrm = D3DXVECTOR3(cosf(a),0.0f,sinf(a));										//v5	p3
		VS[i+6].tex = D3DXVECTOR2((float)(i+2)/4,(float)(i-1)/4);								//v5	p6
		VS[i+7]=VS[i+6];																		//v6	p3
		VS[i+7].pos.y=(float)DR;																//v6	p3
		VS[i+7].tex = D3DXVECTOR2((float)(i+1)/4,(float)(i-1)/4);								//v6	p3
		VS[i+8].pos = D3DXVECTOR3((float)(R+DR)*cosf(a+da),(float)-DR,(float)(R+DR)*sinf(a+da));//v7	p3
		VS[i+8].nrm = D3DXVECTOR3(cosf(a+da),0.0f,sinf(a+da));									//v7	p3
		VS[i+8].tex = D3DXVECTOR2((float)(i+2)/4,(float)i/4);									//v7	p3
		VS[i+9]=VS[i+8];											//v7	p4
		VS[i+10]=VS[i+8];											//v8	p4
		VS[i+10].pos.y=(float)DR;									//v8	p4
		VS[i+10].tex = D3DXVECTOR2((float)(i+1)/4,(float)i/4);		//v8	p4
		VS[i+11]=VS[i+7];											//v6	p4
//======
		VS[i+12]=VS[i+1];											//v2	p5
		VS[i+12].nrm = D3DXVECTOR3(0,1,0);							//v2	p5
		VS[i+13]=VS[i+7];											//v6	p5
		VS[i+13].nrm = D3DXVECTOR3(0,1,0);							//v6	p5
		VS[i+13].tex = D3DXVECTOR2((float)(i+1)/4,(float)(i-1)/4);	//v6	p6
		VS[i+14]=VS[i+4];											//v4	p5
		VS[i+14].nrm = D3DXVECTOR3(0,1,0);							//v4	p5
		VS[i+15]=VS[i+13];											//v6	p6
		VS[i+16]=VS[i+14];											//v4	p6
		VS[i+17]=VS[i+10];											//v8	p6
		VS[i+17].nrm = D3DXVECTOR3(0,1,0);							//v8	p6
//======
		VS[i+18]=VS[i];												//v1	p7
		VS[i+18].nrm=D3DXVECTOR3(0,-1,0);							//v1	p7
		VS[i+18].tex = D3DXVECTOR2((float)(i+3)/4,(float)(i-1)/4);	//v1	p7
		VS[i+19]=VS[i+6];											//v5	p7
		VS[i+19].nrm=D3DXVECTOR3(0,-1,0);							//v5	p7
		VS[i+20]=VS[i+8];											//v7	p7
		VS[i+20].nrm=D3DXVECTOR3(0,-1,0);							//v7	p7
		VS[i+21]=VS[i+18];											//v1	p8
		VS[i+22]=VS[i+20];											//v7	p8
		VS[i+23]=VS[i+3];											//v3	p8
		VS[i+23].nrm=D3DXVECTOR3(0,-1,0);							//v3	p8
		VS[i+23].tex = D3DXVECTOR2((float)(i+3)/4,(float)i/4);		//v3	p8
		i+=24;
	};
};
//-----------------------------------------------------------------------------
// Name: InitGeometry5()
// Desc: Создает неиндексированный массив
//-----------------------------------------------------------------------------
HRESULT InitGeometry5(){
	if(FAILED(p_d3d_Device->CreateVertexBuffer (
		3*NA*8*6*sizeof(MYVERTEX),				//UINT Length
		0,										//DWORD Usage
		D3DFVF_CUSTOMVERTEX,					//DWORD FVF
		D3DPOOL_MANAGED,						//D3DPOOL Pool
		&p_VertexBuffer))) return E_FAIL;		//IDirect3DVertexBuffer8** ppVertexBuffer

	WORD i=0;
	pRVertices=new MYVERTEX[3*NA*8*6];
	SetVS5(R1+DR+DR, &pRVertices[0]);
	SetVS5(R1+DR, &pRVertices[NA*48]);
	SetVS5(R1, &pRVertices[NA*96]);
	float t;
	for(i=NA*48; i<NA*96; i++){
		t=pRVertices[i].pos.x;
		pRVertices[i].pos.x=t*cosf(D3DX_PI/2)-pRVertices[i].pos.y*sinf(D3DX_PI/2);
		pRVertices[i].pos.y=t*sinf(D3DX_PI/2)+pRVertices[i].pos.y*cosf(D3DX_PI/2);
		t=pRVertices[i].nrm.x;
		pRVertices[i].nrm.x=t*cosf(D3DX_PI/2)-pRVertices[i].nrm.y*sinf(D3DX_PI/2);
		pRVertices[i].nrm.y=t*sinf(D3DX_PI/2)+pRVertices[i].nrm.y*cosf(D3DX_PI/2);

		t=pRVertices[i+NA*48].pos.y;
		pRVertices[i+NA*48].pos.y=t*cosf(D3DX_PI/2)-pRVertices[i+NA*48].pos.z*sinf(D3DX_PI/2);
		pRVertices[i+NA*48].pos.z=t*sinf(D3DX_PI/2)+pRVertices[i+NA*48].pos.z*cosf(D3DX_PI/2);
		t=pRVertices[i+NA*48].nrm.y;
		pRVertices[i+NA*48].nrm.y=t*cosf(D3DX_PI/2)-pRVertices[i+NA*48].nrm.z*sinf(D3DX_PI/2);
		pRVertices[i+NA*48].nrm.z=t*sinf(D3DX_PI/2)+pRVertices[i+NA*48].nrm.z*cosf(D3DX_PI/2);
	};
	MYVERTEX* pVertices;
	if(FAILED(p_VertexBuffer->Lock (
		0,										//OffsetToLock -- Offset into the vertex data to lock, in bytes. To lock the entire vertex buffer, specify 0 for both parameters, SizeToLock and OffsetToLock.
		3*NA*8*6*sizeof(MYVERTEX),				//Size of the vertex data to lock, in bytes. To lock the entire vertex buffer, specify 0 for both parameters, SizeToLock and OffsetToLock
		(BYTE**)&pVertices,						//pointer to a memory buffer containing the returned vertex data
		0))) return E_FAIL;						//Combination of zero or more locking flags that describe the type of lock to perform. For this method, the valid flags are: D3DLOCK_DISCARD, D3DLOCK_NO_DIRTY_UPDATE, D3DLOCK_NOSYSLOCK, D3DLOCK_READONLY, D3DLOCK_NOOVERWRITE.
	memcpy (pVertices, pRVertices, 3*NA*8*6*sizeof(MYVERTEX));
	p_VertexBuffer->Unlock();
	return  S_OK;
};
//-----------------------------------------------------------------------------
// Name: SetVS4(int x,y; CUSTOMVERTEX VS)
// Desc: 
//-----------------------------------------------------------------------------
void SetVS4(int x, int y, CUSTOMVERTEX* VS){
	float t1;
	float   u=(float)x-FLAG_SIZE/2;
	float   v=(float)y-FLAG_SIZE/2;
	float txf=FLAG_SIZE/8;
	if(!fPlayMode) {t1=0;} else {t1=timeGetTime()/640.0f;};
    VS->x  = u;
	VS->y  = 128*cos(sqrt(u*u+v*v)/6+t1)/sqrt(u*u+v*v+48)+10;
	VS->z  = v;
	VS->tu = (float)x*txf/(FLAG_SIZE);
	VS->tv = (float)y*txf/(FLAG_SIZE);
};
//-----------------------------------------------------------------------------
// Name: TransGeometry4()
// Desc: Трансформирует неиндексированный массив по времени
//-----------------------------------------------------------------------------
HRESULT TransGeometry4(){
	CUSTOMVERTEX* pVertices;
	if(FAILED(p_VertexBuffer->Lock (0,NUM_FLAG_INDICES*sizeof(CUSTOMVERTEX), 
		(BYTE**)&pVertices,0))) return E_FAIL;
	WORD i=0;
	for(int ix=0; ix<FLAG_SIZE; ix++){
		for(int iy=0; iy<FLAG_SIZE; iy++){
			//10.0f*(cos((float)iy/3.0f)+cos((float)ix/3.0f))
			SetVS4(ix,   iy,   &pVertices[i++]);			//1
			SetVS4(ix+1, iy,   &pVertices[i++]);			//2
			SetVS4(ix,   iy+1, &pVertices[i++]);			//3
			SetVS4(ix+1, iy,   &pVertices[i++]);			//2
			SetVS4(ix+1, iy+1, &pVertices[i++]);			//4
			SetVS4(ix,   iy+1, &pVertices[i++]);			//3
		};
	};
	NormalizeTriangles(NUM_FLAG_INDICES, pVertices);
	p_VertexBuffer->Unlock();
	return  S_OK;
};
//-----------------------------------------------------------------------------
// Name: InitGeometry4()
// Desc: Создает неиндексированный массив
//-----------------------------------------------------------------------------
HRESULT InitGeometry4(){
	//Создаем и копируем данные в буфер индексов
	if(FAILED(p_d3d_Device->CreateVertexBuffer (
		NUM_FLAG_INDICES*sizeof(CUSTOMVERTEX),	//UINT Length
		0,										//DWORD Usage
		D3DFVF_CUSTOMVERTEX,					//DWORD FVF
		D3DPOOL_MANAGED,						//D3DPOOL Pool
		&p_VertexBuffer))) return E_FAIL;		//IDirect3DVertexBuffer8** ppVertexBuffer
	WORD i=0;
	CUSTOMVERTEX* pVertices;
	if(FAILED(p_VertexBuffer->Lock (
		0,										//OffsetToLock -- Offset into the vertex data to lock, in bytes. To lock the entire vertex buffer, specify 0 for both parameters, SizeToLock and OffsetToLock.
		NUM_FLAG_INDICES*sizeof(CUSTOMVERTEX),  //Size of the vertex data to lock, in bytes. To lock the entire vertex buffer, specify 0 for both parameters, SizeToLock and OffsetToLock
		(BYTE**)&pVertices,						//pointer to a memory buffer containing the returned vertex data
		0))) return E_FAIL;						//Combination of zero or more locking flags that describe the type of lock to perform. For this method, the valid flags are: D3DLOCK_DISCARD, D3DLOCK_NO_DIRTY_UPDATE, D3DLOCK_NOSYSLOCK, D3DLOCK_READONLY, D3DLOCK_NOOVERWRITE.
	for(int ix=0; ix<FLAG_SIZE; ix++){
		for(int iy=0; iy<FLAG_SIZE; iy++){
			//10.0f*(cos((float)iy/3.0f)+cos((float)ix/3.0f))
			SetVS4(ix,   iy,   &pVertices[i++]);			//1
			SetVS4(ix+1, iy,   &pVertices[i++]);			//2
			SetVS4(ix,   iy+1, &pVertices[i++]);			//3
			SetVS4(ix+1, iy,   &pVertices[i++]);			//2
			SetVS4(ix+1, iy+1, &pVertices[i++]);			//4
			SetVS4(ix,   iy+1, &pVertices[i++]);			//3
		};
	};
	NormalizeTriangles(NUM_FLAG_INDICES, pVertices);
	p_VertexBuffer->Unlock();
	return  S_OK;
};
//-----------------------------------------------------------------------------
// Name: InitGeometry3()
// Desc: Creates the scene geometry - Indexed surface
//-----------------------------------------------------------------------------
HRESULT InitGeometry3(){
	//Создаем и копируем данные в буфер индексов
	if(FAILED(p_d3d_Device->CreateVertexBuffer (
		sizeof(g_IndexedV),						//UINT Length
		0,										//DWORD Usage
		D3DFVF_CUSTOMVERTEX,					//DWORD FVF
		D3DPOOL_MANAGED,						//D3DPOOL Pool
		&p_VertexBuffer))) return E_FAIL;		//IDirect3DVertexBuffer8** ppVertexBuffer
	VOID* pVertices;
	if(FAILED(p_VertexBuffer->Lock (
		0,										//OffsetToLock -- Offset into the vertex data to lock, in bytes. To lock the entire vertex buffer, specify 0 for both parameters, SizeToLock and OffsetToLock.
		sizeof(g_IndexedV),						//Size of the vertex data to lock, in bytes. To lock the entire vertex buffer, specify 0 for both parameters, SizeToLock and OffsetToLock
		(BYTE**)&pVertices,						//pointer to a memory buffer containing the returned vertex data
		0))) return E_FAIL;						//Combination of zero or more locking flags that describe the type of lock to perform. For this method, the valid flags are: D3DLOCK_DISCARD, D3DLOCK_NO_DIRTY_UPDATE, D3DLOCK_NOSYSLOCK, D3DLOCK_READONLY, D3DLOCK_NOOVERWRITE.
	memcpy (pVertices, g_IndexedV, sizeof(g_IndexedV));
	p_VertexBuffer->Unlock();

	//Создаем и копируем данные в буфер индексов
	if(FAILED(p_d3d_Device->CreateIndexBuffer (
		sizeof(d_Indexses),				//UINT Length,
		0,								//DWORD Usage, Usage can be 0, which indicates no usage value. However, if usage is desired, use a combination of one or more D3DUSAGE constants. It is good practice to match the usage parameter in CreateIndexBuffer with the behavior flags in IDirect3D9::CreateDevice. For more information, see Remarks. 
		D3DFMT_INDEX16,					//D3DFORMAT Format, D3DFMT_INDEX16 Indices are 16 bits each. D3DFMT_INDEX32 Indices are 32 bits each
		D3DPOOL_MANAGED,				//D3DPOOL Pool Member of the D3DPOOL enumerated type, describing a valid memory class into which to place the resource.
		&p_IndexBuffer))) return E_FAIL;//IDirect3DIndexBuffer8** ppIndexBuffer
	VOID* pVerticesI;
	if(FAILED(p_IndexBuffer->Lock (
		0, 
		sizeof(d_Indexses),
		(BYTE**)&pVerticesI, 
		0))) return E_FAIL;
	memcpy (pVerticesI, d_Indexses, sizeof(d_Indexses));
	p_IndexBuffer->Unlock();
	return  S_OK;
};
//-----------------------------------------------------------------------------
// Name: InitGeometry2()
// Desc: Creates the scene geometry - Piramide
//-----------------------------------------------------------------------------
HRESULT InitGeometry2(){
	NormalizeTriangles(sizeof(g_Vertices)/sizeof(CUSTOMVERTEX), g_Vertices);
	if(FAILED(p_d3d_Device->CreateVertexBuffer (
		sizeof(g_Vertices),					//UINT Length
		0,									//DWORD Usage
		D3DFVF_CUSTOMVERTEX,				//DWORD FVF
		D3DPOOL_MANAGED,					//D3DPOOL Pool
		&p_VertexBuffer))) return E_FAIL;	//IDirect3DVertexBuffer8** ppVertexBuffer	
	VOID* pVertices;
	if(FAILED(p_VertexBuffer->Lock (
		0,							//OffsetToLock -- Offset into the vertex data to lock, in bytes. To lock the entire vertex buffer, specify 0 for both parameters, SizeToLock and OffsetToLock.
		sizeof(g_Vertices),			//Size of the vertex data to lock, in bytes. To lock the entire vertex buffer, specify 0 for both parameters, SizeToLock and OffsetToLock
		(BYTE**)&pVertices,			//pointer to a memory buffer containing the returned vertex data
		0))) return E_FAIL;			//Combination of zero or more locking flags that describe the type of lock to perform. For this method, the valid flags are: D3DLOCK_DISCARD, D3DLOCK_NO_DIRTY_UPDATE, D3DLOCK_NOSYSLOCK, D3DLOCK_READONLY, D3DLOCK_NOOVERWRITE.
	memcpy (pVertices, g_Vertices, sizeof(g_Vertices));
	p_VertexBuffer->Unlock();
	return  S_OK;
}
//-----------------------------------------------------------------------------
// Name: InitGeometry()
// Desc: Creates the scene geometry - Indexed surface
//-----------------------------------------------------------------------------
HRESULT InitGeometry(){
	//Создаем и копируем данные в буфер индексов
	if(FAILED(p_d3d_Device->CreateVertexBuffer (
		NUM_FLAG_VERTICES*sizeof(CUSTOMVERTEX), //UINT Length
		0,										//DWORD Usage
		D3DFVF_CUSTOMVERTEX,					//DWORD FVF
		D3DPOOL_MANAGED,						//D3DPOOL Pool
		&p_VertexBuffer))) return E_FAIL;		//IDirect3DVertexBuffer8** ppVertexBuffer
	CUSTOMVERTEX* pVertices;
	if(FAILED(p_VertexBuffer->Lock (
		0,										//OffsetToLock -- Offset into the vertex data to lock, in bytes. To lock the entire vertex buffer, specify 0 for both parameters, SizeToLock and OffsetToLock.
		NUM_FLAG_VERTICES*sizeof(CUSTOMVERTEX), //Size of the vertex data to lock, in bytes. To lock the entire vertex buffer, specify 0 for both parameters, SizeToLock and OffsetToLock
		(BYTE**)&pVertices,						//pointer to a memory buffer containing the returned vertex data
		0))) return E_FAIL;						//Combination of zero or more locking flags that describe the type of lock to perform. For this method, the valid flags are: D3DLOCK_DISCARD, D3DLOCK_NO_DIRTY_UPDATE, D3DLOCK_NOSYSLOCK, D3DLOCK_READONLY, D3DLOCK_NOOVERWRITE.
	for(WORD ix=0; ix<FLAG_SIZE+1; ix++){
		for(WORD iy=0; iy<FLAG_SIZE+1; iy++){
			FLOAT tu = ix/(float)(FLAG_SIZE+1);
			FLOAT tv = iy/(float)(FLAG_SIZE+1);
			pVertices[ix+iy*(FLAG_SIZE+1)].x = (float)(ix - FLAG_SIZE/2);
			pVertices[ix+iy*(FLAG_SIZE+1)].y = 3.0f*(cos((float)iy/3.0f)+cos((float)ix/3.0f));
			pVertices[ix+iy*(FLAG_SIZE+1)].z = (float)(iy - FLAG_SIZE/2);
			pVertices[ix+iy*(FLAG_SIZE+1)].nx = 0.0f;
			pVertices[ix+iy*(FLAG_SIZE+1)].ny = 1.0f;
			pVertices[ix+iy*(FLAG_SIZE+1)].nz = 0.0f;
			pVertices[ix+iy*(FLAG_SIZE+1)].tu = tu;
			pVertices[ix+iy*(FLAG_SIZE+1)].tv = tv;
		};
	};
	p_VertexBuffer->Unlock();
	//Создаем и копируем данные в буфер индексов
	if(FAILED(p_d3d_Device->CreateIndexBuffer (
		NUM_FLAG_INDICES*sizeof(WORD),	//UINT Length,
		0,								//DWORD Usage, Usage can be 0, which indicates no usage value. However, if usage is desired, use a combination of one or more D3DUSAGE constants. It is good practice to match the usage parameter in CreateIndexBuffer with the behavior flags in IDirect3D9::CreateDevice. For more information, see Remarks. 
		D3DFMT_INDEX16,					//D3DFORMAT Format, D3DFMT_INDEX16 Indices are 16 bits each. D3DFMT_INDEX32 Indices are 32 bits each
		D3DPOOL_MANAGED,				//D3DPOOL Pool Member of the D3DPOOL enumerated type, describing a valid memory class into which to place the resource.
		&p_IndexBuffer))) return E_FAIL;//IDirect3DIndexBuffer8** ppIndexBuffer
	WORD* pVerticesI;
	if(FAILED(p_IndexBuffer->Lock (
		0, 
		NUM_FLAG_INDICES*sizeof(WORD), 
		(BYTE**)&pVerticesI, 
		0))) return E_FAIL;
	for(WORD i=0, iy=0; iy<FLAG_SIZE; iy++ ){
		for(WORD ix=0; ix<FLAG_SIZE; ix++){
			pVerticesI[i++] = (ix+0) + (iy+1)*(FLAG_SIZE+1);
			pVerticesI[i++] = (ix+0) + (iy+0)*(FLAG_SIZE+1);
			pVerticesI[i++] = (ix+1) + (iy+0)*(FLAG_SIZE+1);
			pVerticesI[i++] = (ix+0) + (iy+1)*(FLAG_SIZE+1);
			pVerticesI[i++] = (ix+1) + (iy+0)*(FLAG_SIZE+1);
			pVerticesI[i++] = (ix+1) + (iy+1)*(FLAG_SIZE+1);
		};
	};
	p_IndexBuffer->Unlock();
	return  S_OK;
};
//-----------------------------------------------------------------------------
// Name: SetD3DMode()
// Desc: Забиваем параметры заново при переключении режима экрана
//-----------------------------------------------------------------------------
void SetD3DMode(){
	p_d3d_Device->LightEnable (0, true);//и говорим D3D8 о том, что источник света за нулевым идентификатором разрешен и должен работать
	p_d3d_Device->SetRenderState(D3DRS_LIGHTING, true);
	p_d3d_Device->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
	p_d3d_Device->SetRenderState(D3DRS_ZENABLE, TRUE);
	p_d3d_Device->SetRenderState(D3DRS_FILLMODE, CurrentFillMd);
//	p_d3d_Device->SetRenderState(D3DRS_AMBIENT, 0x00502020); // Finally, turn on some ambient light.
	p_d3d_Device->SetRenderState (D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);//Эти две строчки
	p_d3d_Device->SetRenderState (D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);//для прозрачности текстур
};
//-----------------------------------------------------------------------------
// Name: SetUpD3D ()
// Desc: 
//-----------------------------------------------------------------------------
HRESULT SetUpD3D(void){
	if(NULL == (p_d3d = Direct3DCreate8(D3D_SDK_VERSION))) return E_FAIL;
	if(FAILED(p_d3d->GetAdapterDisplayMode(D3DADAPTER_DEFAULT, &d3ddm))) return E_FAIL;
	ZeroMemory (&d3dpp, sizeof(d3dpp));
	d3dpp.BackBufferFormat = d3ddm.Format;
	d3dpp.Windowed = true;
	d3dpp.SwapEffect = D3DSWAPEFFECT_DISCARD;//метод обновления экрана
//	d3dpp.BackBufferCount = 3;//количество вторичных буферов
	d3dpp.EnableAutoDepthStencil = true;//Z-буфер
	d3dpp.AutoDepthStencilFormat = D3DFMT_D16;//Формат Z-буфера, где depthBufferFormat может быть одним из следующих форматов : D3DFMT_D15S1, D3DFMT_D24X8, D3DFMT_D24S8, D3DFMT_D24X4S4, D3DFMT_D32, а можно и D3DFMT_D16
	if(FAILED(p_d3d->CreateDevice(
			D3DADAPTER_DEFAULT,//первичный адаптер
			D3DDEVTYPE_HAL, //Hardware rasterization
			hWnd,//оконный контекст
			D3DCREATE_SOFTWARE_VERTEXPROCESSING,//см. также D3DCREATE_NOWINDOWCHANGES - Indicates that Direct3D must not alter the focus window in any way. Warning  If this flag is set, the application must fully support all focus management events, such as ALT+TAB and mouse click events.
			&d3dpp,
			&p_d3d_Device))){
		if(FAILED(p_d3d->CreateDevice(
				D3DADAPTER_DEFAULT,//первичный адаптер
				D3DDEVTYPE_REF, //Direct3D features are implemented in software; however, the reference rasterizer does make use of special CPU instructions whenever it can
				hWnd,//оконный контекст
				D3DCREATE_SOFTWARE_VERTEXPROCESSING,//Specifies software vertex processing
				&d3dpp,
				&p_d3d_Device)))
			return E_FAIL;
	}
	//p_d3d_Device->SetRenderState (D3DRS_FILLMODE, D3DFILL_SOLID);
	//--Источники света
	ZeroMemory (&light, sizeof(D3DLIGHT8));
	light.Type = D3DLIGHT_DIRECTIONAL;//D3DLIGHT_POINT, D3DLIGHT_SPOT, D3DLIGHT_DIRECTIONAL
	light.Diffuse.r  = 1.0f;
	light.Diffuse.g  = 1.0f;
	light.Diffuse.b  = 1.0f;
	light.Range = 1000.0f;
	SetD3DMode();
	return S_OK;
}
//-----------------------------------------------------------------------------
// Name: PlayWorld()
// Desc: Sets up the world, view, projection, flight, mateials and others transforms.
//-----------------------------------------------------------------------------
VOID PlayWorld(){
	float t1=timeGetTime()/640.0f;
	float t2=t1/2.0f;
	float t3=t1/3.0f;
	// Set up world matrix
    D3DXMATRIX matWorld;
	D3DXMatrixIdentity(&matWorld);//строит единичную матрицу. - вообщето эта строка лишняя
    D3DXMatrixRotationY(&matWorld, 0);
    p_d3d_Device->SetTransform(D3DTS_WORLD, &matWorld);
    // Set up our view matrix.
	D3DXMATRIX  matView;
	D3DXVECTOR3 vEyePt( sin(t2)*(cos(t3)+1.2f)*37.0f, (sin(t3)+1.2f)*25.0f, cos(t2)*(cos(t3)+1.2f)*37);
//	D3DXVECTOR3 vEyePt( sin(t2)*70.0f, 45.0f, cos(t2)*70);
	D3DXVECTOR3 vLookatPt( 0.0f, 0.0f, 0.0f );
	D3DXVECTOR3 vUpVec( 0.0f, 1.0f, 0.0f );
	D3DXMatrixLookAtLH(&matView, &vEyePt, &vLookatPt, &vUpVec);
	p_d3d_Device->SetTransform(D3DTS_VIEW, &matView);
    // projection matrix
    D3DXMATRIX matProj;
    D3DXMatrixPerspectiveFovLH(
		&matProj,	
		D3DX_PI/4,	//Angle of view 
		1.3f,		//Aspect ratio
		1.0f,		//Nearest view plane
		1000.0f);	//Far view plane
    p_d3d_Device->SetTransform(D3DTS_PROJECTION, &matProj);
	//свет
	D3DXVECTOR3 vecDir = D3DXVECTOR3(
		cosf(t1),
        -1.0f,
        sinf(t1));
	D3DXVec3Normalize((D3DXVECTOR3*)&light.Direction, &vecDir);
	p_d3d_Device->SetLight (0, &light);//Связываем источник света с идентификатором 0
	//Трансформируем массив вершин по времени
//	if(fPlayMode) TransGeometry4();
	if(fPlayMode) TransGeometry5();
};
//-----------------------------------------------------------------------------
// Name: RenderScreen ()
// Desc: Draws the scene
//-----------------------------------------------------------------------------
void RenderScreen (void){
	HRESULT hr=p_d3d_Device->TestCooperativeLevel();
	if(hr==D3DERR_DEVICELOST) {
		Sleep( 50 );
		return;
	}
	if(hr==D3DERR_DEVICENOTRESET) {
		p_d3d_Device->Reset(&d3dpp); 
		SetD3DMode();
	};
	if(!fWiteMode) {
		p_d3d_Device->Clear (0, NULL, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER, D3DCOLOR_XRGB (0, 0, 0), 1.0f, 0);
	}else{
		p_d3d_Device->Clear (0, NULL, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER, D3DCOLOR_XRGB (255, 255, 255), 1.0f, 0);
	}
	if (fAlfaBlnd) {
		p_d3d_Device->SetRenderState (D3DRS_ALPHABLENDENABLE, true);
	}else{
		p_d3d_Device->SetRenderState (D3DRS_ALPHABLENDENABLE, false);
	};
	//материалы
	D3DMATERIAL8 mtrl1;
	D3DMATERIAL8 mtrl2;
	D3DMATERIAL8 mtrl3;
	ZeroMemory (&mtrl1, sizeof(D3DMATERIAL8));
	ZeroMemory (&mtrl2, sizeof(D3DMATERIAL8));
	ZeroMemory (&mtrl3, sizeof(D3DMATERIAL8));
	p_d3d_Device->BeginScene ();//далее у нас идет строка начала сцены 
	PlayWorld();
//	p_d3d_Device->SetStreamSource (0, p_VertexBuffer, sizeof(CUSTOMVERTEX));
	p_d3d_Device->SetStreamSource (0, p_VertexBuffer, sizeof(MYVERTEX));
	p_d3d_Device->SetVertexShader (D3DFVF_CUSTOMVERTEX);
	mtrl1.Diffuse.r = mtrl1.Ambient.r = 1.0f;
	mtrl1.Diffuse.g = mtrl1.Ambient.g = 1.0f;
	mtrl1.Diffuse.b = mtrl1.Ambient.b = 0.0f;
	mtrl1.Diffuse.a = mtrl1.Ambient.a = 1.0f;
	p_d3d_Device->SetMaterial (&mtrl1);
	//Текстура
	p_d3d_Device->SetTexture(0, p_Texture_001);
	p_d3d_Device->SetTextureStageState(0, D3DTSS_MAGFILTER, CurrentFilter);
	p_d3d_Device->SetTextureStageState(0, D3DTSS_MINFILTER, CurrentFilter);
	p_d3d_Device->SetTextureStageState(0, D3DTSS_MIPFILTER, CurrentFilter);	
//	p_d3d_Device->SetTextureStageState (0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
//	p_d3d_Device->SetTextureStageState (0, D3DTSS_COLOROP, D3DTOP_SELECTARG1);
//	p_d3d_Device->SetTextureStageState (0, D3DTSS_COLOROP, D3DTOP_MODULATE);
	p_d3d_Device->DrawPrimitive (D3DPT_TRIANGLELIST, 0, NA*16);//Это к примеру №5
	mtrl2.Diffuse.r = mtrl1.Ambient.r = 0.0f;
	mtrl2.Diffuse.g = mtrl1.Ambient.g = 1.0f;
	mtrl2.Diffuse.b = mtrl1.Ambient.b = 1.0f;
	mtrl2.Diffuse.a = mtrl1.Ambient.a = 1.0f;
	p_d3d_Device->SetMaterial (&mtrl2);
	p_d3d_Device->DrawPrimitive (D3DPT_TRIANGLELIST, NA*48, NA*16);//Это к примеру №5
	mtrl3.Diffuse.r = mtrl1.Ambient.r = 1.0f;
	mtrl3.Diffuse.g = mtrl1.Ambient.g = 0.0f;
	mtrl3.Diffuse.b = mtrl1.Ambient.b = 1.0f;
	mtrl3.Diffuse.a = mtrl1.Ambient.a = 1.0f;
	p_d3d_Device->SetMaterial (&mtrl3);
	p_d3d_Device->DrawPrimitive (D3DPT_TRIANGLELIST, NA*96, NA*16);//Это к примеру №5

//	p_d3d_Device->DrawPrimitive (D3DPT_TRIANGLELIST, 0, NUM_FLAG_INDICES/3);//Это к примеру №4
//	p_d3d_Device->DrawPrimitive (D3DPT_TRIANGLELIST, 0, 4);
//	p_d3d_Device->SetIndices (p_IndexBuffer, 0);
//	p_d3d_Device->DrawIndexedPrimitive (
//		D3DPT_TRIANGLELIST, //D3DPT_POINTLIST = 1, D3DPT_LINELIST = 2, D3DPT_LINESTRIP = 3, D3DPT_TRIANGLELIST = 4, D3DPT_TRIANGLESTRIP = 5, D3DPT_TRIANGLEFAN = 6
//		0,					//INT BaseVertexIndex
//	NUM_FLAG_VERTICES,	//Number of vertices used during this call
////		5,					//Number of vertices used during this call
//		0,					//Index of the first index to use when accesssing the vertex buffer
//	NUM_FLAG_INDICES/3);//Number of primitives to render 
////		4);					//Number of primitives to render 
//	if (fAlfaBlnd) p_d3d_Device->SetRenderState (D3DRS_ALPHABLENDENABLE, false);
	p_d3d_Device->EndScene();
	p_d3d_Device->Present (NULL, NULL, NULL, NULL);
};
//-----------------------------------------------------------------------------
// Name: SetTitle()
// Desc: 
//-----------------------------------------------------------------------------
void SetTitle(){
	strcpy(AppTitle,"Grid 3D ");
	switch (CurrentFilter){
	case D3DTEXF_NONE:
		strcat(AppTitle,"(Filter=D3DTEXF_NONE ");
		break;
	case D3DTEXF_POINT:
		strcat(AppTitle,"(Filter=D3DTEXF_POINT ");
		break;
	case D3DTEXF_LINEAR:
		strcat(AppTitle,"(Filter=D3DTEXF_LINEAR ");
		break;
	case D3DTEXF_ANISOTROPIC:
		strcat(AppTitle,"(Filter=D3DTEXF_ANISOTROPIC ");
		break;
	};
	switch (CurrentFillMd){
	case D3DFILL_POINT:
		strcat(AppTitle,"FillMode=D3DFILL_POINT)");
		break;
	case D3DFILL_WIREFRAME:
		strcat(AppTitle,"FillMode=D3DFILL_WIREFRAME)");
		break;
	case D3DFILL_SOLID:
		strcat(AppTitle,"FillMode=D3DFILL_SOLID)");
		break;
	};
	strcat(AppTitle," Copyright (c) Grushetsky V.A.");
};
//-----------------------------------------------------------------------------
// Name: WindowProc ()
// Desc: 
// hWnd - дескриптор окна (слово), messagew - сообщение (слово), Param - необязательный параметр типа слова, lParam - необязательный параметр типа двойного слова
//-----------------------------------------------------------------------------
LRESULT CALLBACK WindowProc (HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam){
	BOOL modechangefl;
	switch (message){
	case WM_KEYDOWN:{
		int VK = (int)wParam;
		if (VK==VK_RETURN){//Переключение из окна на полный экран и обратно
			d3dpp.Windowed=isFullScreen;
			isFullScreen=!isFullScreen;
			d3dpp.SwapEffect = D3DSWAPEFFECT_DISCARD;
			d3dpp.BackBufferWidth = d3ddm.Width;
			d3dpp.BackBufferHeight = d3ddm.Height;
//			d3dpp.BackBufferCount = 3;//количество вторичных буферов
			d3dpp.FullScreen_RefreshRateInHz = d3ddm.RefreshRate;//частоту обновления экрана
			d3dpp.BackBufferFormat = d3ddm.Format;
			d3dpp.EnableAutoDepthStencil = true;
			d3dpp.AutoDepthStencilFormat = D3DFMT_D16;
			if (isFullScreen){
				SetWindowLong (hWnd, GWL_STYLE, WS_POPUP);
				SetWindowPos (hWnd, 0, 0, 0, d3ddm.Width, d3ddm.Height, 0);
				SetCursor (NULL);
			}else{
				SetWindowLong (hWnd, GWL_STYLE, GetWindowLong(hWnd, GWL_STYLE) | WS_CAPTION | WS_MINIMIZEBOX | WS_SYSMENU | WS_MAXIMIZEBOX);
				SetWindowPos (hWnd, 0, 0, 0, 640, 480, 0);
				SetCursor (LoadCursor (NULL, IDC_ARROW));
			};
			p_d3d_Device->Reset(&d3dpp);
			SetD3DMode();
		};
		modechangefl=true;
		if (VK==VK_F8)  {fPlayMode=!fPlayMode;} else
		if (VK==VK_F9)  {fWiteMode=!fWiteMode;} else
		if (VK==VK_F11) {fAlfaBlnd=!fAlfaBlnd;} else
		switch(VK){
		case VK_F1:
			CurrentFilter = D3DTEXF_NONE;
			break;
		case VK_F2:
			CurrentFilter = D3DTEXF_POINT;
			break;
		case VK_F3:
			CurrentFilter = D3DTEXF_LINEAR;
			break;
		case VK_F4:
			CurrentFilter = D3DTEXF_ANISOTROPIC;
			break;
		case VK_F5:
			CurrentFillMd = D3DFILL_POINT;
			break;
		case VK_F6:
			CurrentFillMd = D3DFILL_WIREFRAME;
			break;
		case VK_F7:
			CurrentFillMd = D3DFILL_SOLID;
			break;
		default:
			modechangefl=false;
		};
		if (modechangefl){
			SetTitle();
			SetWindowText(hWnd, AppTitle);
			p_d3d_Device->SetRenderState (D3DRS_FILLMODE, CurrentFillMd);
		};
		break;
	};
	case WM_DESTROY:
		PostQuitMessage (0);
		break;
//	case WM_SETCURSOR://убирает курсор
//		SetCursor (NULL);
//		break; 
	};
	return DefWindowProc(hWnd, message, wParam, lParam);//вызов дескриптора первоначального сообщения
};
//-----------------------------------------------------------------------------
// Name: WindowInit ()
// Desc: 
//-----------------------------------------------------------------------------
bool WindowInit (HINSTANCE hThisInst, int nCmdShow){
	WNDCLASS		    wcl;
	wcl.hInstance		= hThisInst;
	wcl.lpszClassName	= APPNAME;
	wcl.lpfnWndProc		= WindowProc;
	wcl.style			= 0;//CS_VREDRAW|CS_HREDRAW|CS_OWNDC;
	wcl.hIcon			= LoadIcon (hThisInst, IDC_ICON);
	wcl.hCursor			= LoadCursor (hThisInst, IDC_ARROW);//в этой функции вместо дескриптора инстанции можно поставить нуль (возвр также в AX)
	wcl.lpszMenuName	= NULL;
	wcl.cbClsExtra		= 0;
	wcl.cbWndExtra		= 0;
	wcl.hbrBackground	= (HBRUSH) GetStockObject (BLACK_BRUSH);
	RegisterClass (&wcl);
//	int sx = GetSystemMetrics (SM_CXSCREEN);
//	int sy = GetSystemMetrics (SM_CYSCREEN);
//	hWnd = CreateWindowEx(
//		WS_EX_TOPMOST,
////	WS_OVERLAPPED | WS_SYSMENU,
////	WS_OVERLAPPEDWINDOW|WS_VISIBLE,
//		WS_CAPTION | WS_MINIMIZEBOX | WS_SYSMENU | WS_MAXIMIZEBOX,
	hWnd = CreateWindow(
		APPNAME,
		AppTitle,
		WS_CAPTION | WS_MINIMIZEBOX | WS_SYSMENU | WS_MAXIMIZEBOX,
//		sx/2-512, sy/2-384,
		0, 0,
		640,
		480,
		NULL,
		NULL,
		hThisInst,
		NULL);
	if(!hWnd) return false;	
	return true;
};
//-----------------------------------------------------------------------------
// Name: WinMain ()
// Desc: Точка Входа
//-----------------------------------------------------------------------------
int APIENTRY WinMain (HINSTANCE hThisInst, HINSTANCE hPrevInst, LPSTR lpCmdLine, int nCmdShow){
	MSG msg;
	HRESULT d3derrcode;
	char d3derrmsg[64];
	hTInst=hThisInst;
	hPInst=hPrevInst;
	CmdShow=nCmdShow;
	CurrentFilter = D3DTEXF_NONE;
	CurrentFillMd = D3DFILL_SOLID;
	SetTitle();
	if(!WindowInit (hThisInst, nCmdShow)) return false;
	d3derrcode=SetUpD3D();
	if(!d3derrcode)
		d3derrcode=InitGeometry5();
	if (d3derrcode){
		sprintf(d3derrmsg, "Initialization procedure return an error code: %dx", d3derrcode);
		MessageBox(NULL, d3derrmsg, "ERROR", MB_ICONERROR);
		return 1;
	};
//	if(FAILED(D3DXCreateTextureFromFile(p_d3d_Device, "005.png", &p_Texture_001))){
	if(FAILED(D3DXCreateTextureFromFileEx (
			p_d3d_Device, 
			"005.png", 
			0, 0, 0, 0,
			D3DFMT_UNKNOWN, 
			D3DPOOL_MANAGED, 
			D3DX_DEFAULT, 
			D3DX_DEFAULT, 
			0xFF000000,
			NULL, 
			NULL, 
			&p_Texture_001))){
		MessageBox(NULL, "Не найдена текстура 005.png", "ERROR", MB_ICONERROR);
		return 2;
	}
	ShowWindow (hWnd, nCmdShow);
	UpdateWindow (hWnd);
	SetCursor (LoadCursor (NULL, IDC_ARROW));
	while (1)	{
		if(PeekMessage (&msg, NULL, 0, 0, PM_NOREMOVE)){//проверяем пришло ли сообщение
			if(!GetMessage (&msg, NULL, 0, 0)) break;//Если возвращается нуль то завершаемся
			TranslateMessage (&msg);//перевод сообщения
			DispatchMessage (&msg);//пересылка сообщения оконной процедуре
		}else RenderScreen ();
	};
	DestroyDirect3D8();
	return 0;
};
//----------------------------------------------------------------------------- EOF
