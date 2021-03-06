#include <windows.h>
#include <objbase.h>
#include <glad/glad.h>
#include <GL/wglext.h>
#include <stdint.h>
#include "utility.h"
#include "shader.h"
#include "renderer.h"
#include "math.h"
#include "player.h"
#include "camera.h"
#include "terrain.h"
#include "enemy.h"
#define global_variable static
#define WNDWIDTH 1000
#define WNDHEIGHT 800

BOOL appRunnig = FALSE;
Matrix view;
Matrix proj;
unsigned int viewLocation;

void InitOpenglContext(HWND hWnd)
{
    PIXELFORMATDESCRIPTOR pfd =
    {
	    sizeof(PIXELFORMATDESCRIPTOR),
	    1,
	    PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER,    // Flags
	    PFD_TYPE_RGBA,        // The kind of framebuffer. RGBA or palette.
	    32,                   // Colordepth of the framebuffer.
	    0, 0, 0, 0, 0, 0,
	    0,
	    0,
	    0,
	    0, 0, 0, 0,
	    24,                   // Number of bits for the depthbuffer
	    8,                    // Number of bits for the stencilbuffer
	    0,                    // Number of Aux buffers in the framebuffer.
	    PFD_MAIN_PLANE,
	    0,
	    0, 0, 0
    };


    // create openg gl context in win32 
    HDC device_context = GetDC(hWnd);
    int pixel_format = ChoosePixelFormat(device_context, &pfd);
    SetPixelFormat(device_context, pixel_format, &pfd);
    HGLRC opengl_render_context_temp = wglCreateContext(device_context);
    wglMakeCurrent(device_context, opengl_render_context_temp);
        
    int Attributes[] = {
        WGL_CONTEXT_MAJOR_VERSION_ARB, 3,
        WGL_CONTEXT_MINOR_VERSION_ARB, 3,
        WGL_CONTEXT_FLAGS_ARB, WGL_CONTEXT_CORE_PROFILE_BIT_ARB,
        0
    };
        
    PFNWGLCREATECONTEXTATTRIBSARBPROC wglCreateContextAttribsARB = 
        (PFNWGLCREATECONTEXTATTRIBSARBPROC)wglGetProcAddress("wglCreateContextAttribsARB");
    HGLRC opengl_render_context;
    if(wglCreateContextAttribsARB)
    {
        opengl_render_context = wglCreateContextAttribsARB(device_context, NULL, Attributes);
        wglMakeCurrent(NULL, NULL);
        wglDeleteContext(opengl_render_context_temp);
        wglMakeCurrent(device_context, opengl_render_context);

        PFNWGLSWAPINTERVALEXTPROC wglSwapIntervalEXT = 
            (PFNWGLSWAPINTERVALEXTPROC)wglGetProcAddress("wglSwapIntervalEXT");
        wglSwapIntervalEXT(1);
    }

    if(!gladLoadGL())
    {
        OutputDebugString("ERROR::INITIALIZING::GLAD\n");
    }

    ReleaseDC(hWnd, device_context);
}


LRESULT CALLBACK WindowProc(HWND hwnd, UINT Msg, WPARAM wParam, LPARAM lParam)
{
    LRESULT result;
    switch(Msg)
    {
        case WM_CLOSE:
        {
            appRunnig = FALSE;
        }break;
        case WM_DESTROY:
        {
            appRunnig = FALSE;
        }break;
        default:
        {
            result = DefWindowProc(hwnd, Msg, wParam, lParam); 
        }break;
    }
    return(result);
}

static Shader mesh_shader;

static Mesh bullet;
static Mesh bullets[10];
static Player player;

static Controller controller;
static ThirdPersonCamera third_person_camera = ThirdPersonCamera(&player.position, &player.current_rotation);
static Terrain terrain;

void SetUpStuff()
{   
    shader_load(&mesh_shader, "./code/mesh_shader.vert", "./code/mesh_shader.frag");
    LoadOBJFileIndex(&bullet, "./data/bullet.obj", "./data/bullet.bmp"); 

    Vec3 player_pos = {6.0f, 0.0f, 10.0f};
    init_player(&player, player_pos, mesh_shader.ID, 7.0f, 200.0f);
    for(int i = 0; i < 10; i++)
    {
        bullets[i] = bullet;
    }
    init_player_projectil(&player, bullets, 10, 20, 20.0f);

    Vec3 terrain_position = {0.0f, 0.0f, 0.0f};
    generate_terrain(&terrain, terrain_position, 20, 20, 4, "./data/grass.bmp");
    push_to_render(&terrain, mesh_shader.ID);
    setup_terrain(&player, &terrain, mesh_shader); 

    shader_use(mesh_shader.ID);
    proj = get_projection_perspective_matrix(45.0f*PI/180.0f, 800.0f / 600.0f, 0.1f, 100.0f);
    unsigned int projLocation  = glGetUniformLocation(mesh_shader.ID, "proj");
    viewLocation  = glGetUniformLocation(mesh_shader.ID, "view");
    glUniformMatrix4fv(projLocation, 1, GL_FALSE, proj.m[0]);
    unsigned  int texture1Location = glGetUniformLocation(mesh_shader.ID, "texture1");
    glUniform1i(texture1Location, 0);
}

void UpdateStuff(float deltaTime)
{
    player_input_handler(&player, &controller); 
    process_player_movement(&player, deltaTime);
    process_camera_movement(&third_person_camera, deltaTime);
    update_fireballs(&player, deltaTime);
    terrain_coilitions(&player, deltaTime);

    view = third_person_camera.view;
    glUniformMatrix4fv(viewLocation, 1, GL_FALSE, view.m[0]);
}

int WinMain(HINSTANCE hInstance,
            HINSTANCE hPrevInstance,
            LPSTR     lpCmdLine,
            int       nCmdShow)
{

    WNDCLASS wc;
    ZeroMemory(&wc, sizeof(wc));
    wc.style         = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc   = WindowProc;
    wc.hInstance     = hInstance; 
    wc.lpszClassName = "FrameClass";

    if(!RegisterClassA(&wc))
    {
        OutputDebugString("FAILED register class\n");
        return(1);    
    }

    RECT wr;
	wr.left = 0;
	wr.right = WNDWIDTH;
	wr.top = 0;
	wr.bottom = WNDHEIGHT;
	AdjustWindowRect(&wr, WS_OVERLAPPED, FALSE);

    HWND hWnd = CreateWindowA("FrameClass", "Role3DGame",
                  WS_OVERLAPPEDWINDOW,
                  0, 0,
                  wr.right - wr.left, wr.bottom - wr.top,
                  NULL, NULL,
                  hInstance,
                  NULL);
    LARGE_INTEGER perfCountFrequency;
    QueryPerformanceFrequency(&perfCountFrequency);
    uint64_t frequency = (uint64_t)perfCountFrequency.QuadPart;

    if(hWnd)
    {
        appRunnig = TRUE;
        ShowWindow(hWnd, SW_SHOW);
        CoInitialize(nullptr);
        LARGE_INTEGER lastCounter;
        QueryPerformanceCounter(&lastCounter);

        HDC device_context = GetDC(hWnd);
        
        InitOpenglContext(hWnd); 
        glEnable(GL_CULL_FACE);
        glCullFace(GL_FRONT);
        glViewport(0, 0, WNDWIDTH, WNDHEIGHT);
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
        glEnable(GL_DEPTH_TEST); 
        
        SetUpStuff();

        while(appRunnig == TRUE)
        {
            MSG  msg;
            static float lastTime = (float)timeGetTime();
            if(PeekMessageA(&msg, hWnd, 0, 0, PM_REMOVE))
            {            
                TranslateMessage(&msg); 
                DispatchMessage(&msg); 
            }
            else
            {
                LARGE_INTEGER endCounter;
                QueryPerformanceCounter(&endCounter);
                uint64_t counterElapsed =  endCounter.QuadPart - lastCounter.QuadPart; 
                float deltaTime = (counterElapsed / (float)frequency); 

                glClearColor(0.2f, 0.5f, 1.0f, 1.0f);
                glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

                UpdateStuff(deltaTime);
                render(deltaTime); 

                SwapBuffers(device_context);
                lastCounter = endCounter; 
        
            }
        }
        ReleaseDC(hWnd, device_context);
        clear_render();
    }
    else
    {
        return(1);
    }
    return 0;
}





