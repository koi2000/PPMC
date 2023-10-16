/*****************************************************************************
* Copyright (C) 2011 Adrien Maglo and Clément Courbet
*
* This file is part of PPMC.
*
* PPMC is free software: you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation, either version 3 of the License, or
* (at your option) any later version.
*
* PPMC is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with PPMC.  If not, see <http://www.gnu.org/licenses/>.
*****************************************************************************/

#include "mymesh.h"

#include <stdio.h>
#include <assert.h>
#include <string.h>

#include <GL/glut.h>

#include "configuration.h"


// MOUSE INTERFACE
static int LeftButtonDown = 0;
static int MiddleButtonDown = 0;
static int RightButtonDown = 0;
static int OldX, OldY;
static float Elevation = 0;
static float Azimuth = 0;
static float DistX = 0;
static float DistY = 0;
static float DistZ = 2;
static float camMotionStep = 0.1;

// VISUALIZATION SETTINGS

static float boundingBoxScale = 1.0f;
static float boundingBoxTranslateX = 0.0f;
static float boundingBoxTranslateY = 0.0f;
static float boundingBoxTranslateZ = 0.0f;
static float f_diag = 0;

static float halfedgeOffsetNormal = 0.001;
static float halfedgeOffsetTangential = 0.003;

// GLOBAL CONTROL VARIABLES

static int WindowW = 1024, WindowH = 768;
//static int WindowW=800, WindowH=600;
static int InteractionMode = 0;
static int WireFrameOn = 1;
static int HalfedgesOn = 0;
static int VerticesOn = 0;
static int FacesOn = 1;
static int PrintToFileOn = 0;
static int GUIhelpOn = 0;

static unsigned char *Framebuffer = 0;
static const char *PrintFileName = "frame";
static int Time = 0;
static const char *cameraSaveFilename = "cameraSave";

// COLORS
//static float colours_white[4]={1.0f, 1.0f, 1.0f, 1.0f};
static float colours_blue[4] = {0.8f, 0.8f, 1.0f, 1.0f};
static float colours_green[4] = {0.8f, 1.0f, 0.8f, 1.0f};
static float colours_red[4] = {1.0f, 0.8f, 0.8f, 1.0f};
static float colours_grey[4] = {0.5f, 0.5f, 0.5f, 1.0f};

// GUI help
static const char *const GUIhelp[] =
        {
                "GUI command help",
                "",
                "Algorithm",
                "    s : Perform one step.",
                "    b : Perform a batch of steps.",
                "    B : Perform all the remaining steps.",
                "Camera",
                "    > : zoom in.",
                "    < : zoom out.",
                "    k : Save the camera position.",
                "    l : load the camera postion.",
                "    space bar : change mode (rotate, translate, zoom).",
                "Display",
                "    e : Draw edges on/off.",
                "    f : Draw faces on/off.",
                "    v : Draw vertices on/off.",
                "    h : Draw halfedge on/off.",
                "    1 : Increase the tangential offset for the halfhedge drawing.",
                "    2 : Decrease the tangential offset for the halfhedge drawing.",
                "    4 : Increase the normal offset for the halfhedge drawing.",
                "    5 : Decrease the normal offset for the halfhedge drawing.",
                "Misc.",
                "    q : Quit the program.",
                "    d : Save a snapshot.",
                "    ? : Display this help on/off."
        };


//
//algorithmic stuff
//

//global variables
MyMesh *currentMesh = NULL;


static void SavePPM(char *FileName, unsigned char *Colour, int Width, int Height) {
    FILE *fp = fopen(FileName, "wb");
    fprintf(fp, "P6\n%d %d\n255\n", Width, Height);
    int NumRowPixels = Width * 3;
    for (int i = (Height - 1) * Width * 3; i >= 0; i -= (Width * 3)) {
        fwrite(&(Colour[i]), 1, NumRowPixels, fp);
    }
    fclose(fp);
}


static void InitLight() {
    float intensity[] = {1, 1, 1, 1};
    float position[] = {1, 1, 5, 0}; // directional behind the viewer
    glLightfv(GL_LIGHT0, GL_DIFFUSE, intensity);
    glLightfv(GL_LIGHT0, GL_SPECULAR, intensity);
    glLightfv(GL_LIGHT0, GL_POSITION, position);
    glLightModeli(GL_LIGHT_MODEL_LOCAL_VIEWER, GL_FALSE);
    glLightModeli(GL_LIGHT_MODEL_TWO_SIDE, GL_TRUE);
}

static void myReshape(int w, int h) {
    glutReshapeWindow(WindowW, WindowH);
}

static void myIdle() {
    glutPostRedisplay();
}

static void myMouseFunc(int button, int state, int x, int y) {
    OldX = x;
    OldY = y;
    if (button == GLUT_LEFT_BUTTON) {
        LeftButtonDown = !state;
        MiddleButtonDown = 0;
        RightButtonDown = 0;
    } else if (button == GLUT_RIGHT_BUTTON) {
        LeftButtonDown = 0;
        MiddleButtonDown = 0;
        RightButtonDown = !state;
    } else if (button == GLUT_MIDDLE_BUTTON) {
        LeftButtonDown = 0;
        MiddleButtonDown = !state;
        RightButtonDown = 0;
    }
}

static void myMotionFunc(int x, int y) {
    float RelX = (x - OldX) / (float) glutGet((GLenum) GLUT_WINDOW_WIDTH);
    float RelY = (y - OldY) / (float) glutGet((GLenum) GLUT_WINDOW_HEIGHT);
    OldX = x;
    OldY = y;
    if (LeftButtonDown) {
        if (InteractionMode == 0) {
            Azimuth += (RelX * 180);
            Elevation += (RelY * 180);
        } else if (InteractionMode == 1) {
            DistX -= RelX * camMotionStep;
            DistY += RelY * camMotionStep;
        } else if (InteractionMode == 2) {
            if (DistZ - RelY * DistZ > 0)
                DistZ -= RelY * DistZ;
        }
    } else if (MiddleButtonDown) {
        DistX -= RelX * camMotionStep;
        DistY += RelY * camMotionStep;
    }
    glutPostRedisplay();
}

void myKeyboard(unsigned char Key, int x, int y) {
    FILE *hf;

    switch (Key) {
        case 'q':
        case 27:
            exit(0);
            break;
        case ' ': // rotate, translate, or zoom
            if (InteractionMode == 2) {
                InteractionMode = 0;
            } else {
                InteractionMode += 1;
            }
            glutPostRedisplay();
            break;
        case '>': //zoom in
            if (DistZ - camMotionStep * 0.1 > 0)
                DistZ -= camMotionStep * 0.1;
            break;
        case '<': //zoom out
            DistZ += camMotionStep * 0.1;
            break;
        case '1':
            halfedgeOffsetTangential *= 1.1f;
            break;
        case '2':
            halfedgeOffsetTangential /= 1.1f;
            break;
        case '4':
            halfedgeOffsetNormal *= 1.1f;
            break;
        case '5':
            halfedgeOffsetNormal /= 1.1f;
            break;
        case 'e':
            WireFrameOn = !WireFrameOn;
            glutPostRedisplay();
            break;
        case 'f':
            FacesOn = !FacesOn;
            glutPostRedisplay();
            break;
        case 'v':
            VerticesOn = !VerticesOn;
            glutPostRedisplay();
            break;
        case 'h':
            HalfedgesOn = !HalfedgesOn;
            glutPostRedisplay();
            break;
        case 'd':
            PrintToFileOn = 1;
            if (Framebuffer) delete[] Framebuffer;
            Framebuffer = new unsigned char[WindowW * WindowH * 3];
            fprintf(stderr, "print_to_file %d\n", PrintToFileOn);
            break;
        case 's':
            currentMesh->stepOperation();
            glutPostRedisplay();
            break;
        case 'b':
            currentMesh->batchOperation();
            glutPostRedisplay();
            break;
        case 'B':
            currentMesh->completeOperation();
            glutPostRedisplay();
            break;
        case '?':
            GUIhelpOn = !GUIhelpOn;
            break;
        case 'k':
            printf("Camera position saved: %g %g %g %g %g\n", Azimuth, Elevation, DistX, DistY, DistZ);
            hf = fopen(cameraSaveFilename, "w");
            fprintf(hf, "%g %g %g %g %g\n", Azimuth, Elevation, DistX, DistY, DistZ);
            fclose(hf);
            break;
        case 'l':
            hf = fopen(cameraSaveFilename, "r");
            int i_ret = fscanf(hf, "%g %g %g %g %g", &Azimuth, &Elevation, &DistX, &DistY, &DistZ);
            fclose(hf);
            if (i_ret == 5)
                printf("Camera position loaded: %g %g %g %g %g\n", Azimuth, Elevation, DistX, DistY, DistZ);
            else
                std::cout << "Can't load the camera postion." << std::endl;
            glutPostRedisplay();
            break;
    };
}


void renderString(const char *s) {
    while (*s) {
        glutBitmapCharacter(GLUT_BITMAP_9_BY_15, *s);
        s++;
    }
}


void displayMessage() {
    glColor3f(0.3f, 0.3f, 0.3f);  // Set colour to grey
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    gluOrtho2D(0.0f, 1.0f, 0.0f, 1.0f);
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();
    glRasterPos2f(0.03f, 0.95f);

    if (InteractionMode == 0)
        renderString("Rotate");
    else if (InteractionMode == 1)
        renderString("Translate");
    else
        renderString("Zoom");

    if (!GUIhelpOn) {
        glRasterPos2f(0.03f, 0.05f);
        renderString("? : display GUI command help");
    } else {
        for (unsigned i = 0; i < 25; ++i) {
            glRasterPos2f(0.03f, 0.90f - 0.02 * i);
            renderString(GUIhelp[i]);
        }
    }

    glPopMatrix();
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
}

void drawHalfedge(MyMesh::Halfedge_const_handle h) {
    //is the halfedge associated with a face ?
    bool boundary = h->is_border();

    //the two endpoints:
    MyMesh::Vertex_const_handle vb = h->vertex();
    MyMesh::Vertex_const_handle va = h->opposite()->vertex();
    Point b = vb->point();
    Point a = va->point();

    //compute the  tangent <t> to the halfedge:
    Vector t(a, b);
    t = t / sqrt(t * t);

    //compute the normal <n> to the halfedge:
    Vector n;
    if (!boundary) {
        //the next point
        Point c = h->next()->vertex()->point();
        n = Vector(b, c);
    } else {
        //use the opposite face
        Point c = h->opposite()->next()->vertex()->point();
        n = Vector(c, b);
    }
    n = n - t * (n * t);
    n = n / sqrt(n * n);

    //offset the two points
    Point ap = (va->halfedge() == h) ? a + n * halfedgeOffsetNormal : (a + t * halfedgeOffsetTangential +
                                                                       n * halfedgeOffsetNormal);
    Point bp = b - t * halfedgeOffsetTangential + n * halfedgeOffsetNormal;
    //arrow
    Point dp = bp - t * (2.0 * halfedgeOffsetTangential) + n * (2.0 * halfedgeOffsetNormal);

    glBegin(GL_LINE_STRIP);
    if (h->isInNormalQueue())
        glColor3f(1.0f, 0.0f, 0.0f);
    else if (h->isInProblematicQueue())
        glColor3f(0.0f, 0.0f, 1.0f);
    else if (h->isProcessed())
        glColor3f(1, 1, 1);
    else
        glColor3f(0.0f, 0.0f, 0.0f);
    glVertex3f(ap[0], ap[1], ap[2]);
    glVertex3f(bp[0], bp[1], bp[2]);
    glVertex3f(dp[0], dp[1], dp[2]);
    glEnd();

    if (!boundary && (h->facet()->halfedge() == h)) {
        Point mp = ap + t * (2.0 * halfedgeOffsetTangential);
        Point mp2 = mp + n * (2.0 * halfedgeOffsetNormal);
        glBegin(GL_LINES);
        glVertex3f(mp[0], mp[1], mp[2]);
        glVertex3f(mp2[0], mp2[1], mp2[2]);
        glEnd();
    }


}


void myDisplay() {
    glClearColor(1.0f, 1.0f, 1.0f, 1.0f);

    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glViewport(0, 0, WindowW, WindowH);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluPerspective(30.0f, (float) WindowW / WindowH, f_diag / 100, f_diag * 10); // 0.0625f 1000.0f

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    gluLookAt(DistX, DistY, DistZ, DistX, DistY, 0, 0, 1, 0);

    glRotatef(Elevation, 1, 0, 0);
    glRotatef(Azimuth, 0, 1, 0);

    glTranslatef(boundingBoxTranslateX, boundingBoxTranslateY, boundingBoxTranslateZ);
    glScalef(boundingBoxScale, boundingBoxScale, boundingBoxScale);


    glEnable(GL_DEPTH_TEST);
    glEnable(GL_LIGHTING);
    glEnable(GL_LIGHT0);
    glEnable(GL_NORMALIZE);

    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

    glEnable(GL_POLYGON_OFFSET_FILL);
    glPolygonOffset(1.0, 1.0);

    if (FacesOn) {
        for (MyMesh::Face_const_iterator fit = currentMesh->facets_begin(); fit != currentMesh->facets_end(); ++fit) {
            assert(fit->facet_degree() > 2);
            if (fit->isProcessed())
                glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, colours_grey);
            else {
                if (fit->isSplittable())
                    glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, colours_green);
                else if (fit->isUnsplittable())
                    glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, colours_red);
                else
                    glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, colours_blue);
            }
            glBegin(GL_POLYGON);

#if 0
            // Compute a normal per vertex by summing the computed normals of its incident faces...
            // This is of course very slow.
            MyMesh::Halfedge_around_facet_const_circulator fhit(fit->facet_begin()), end(fhit);
            do
            {
                Vector n = currentMesh->computeVertexNormal(fhit);
                glNormal3f(n[0], n[1], n[2]);

                Point p = fhit->vertex()->point();
                glVertex3f(p[0], p[1], p[2]);
            }
            while(++fhit != end);
#else
            //all the vertices will have the same normal
            Vector n = currentMesh->computeNormal(fit);
            glNormal3f(n[0], n[1], n[2]);
            MyMesh::Halfedge_around_facet_const_circulator fhit(fit->facet_begin()), end(fhit);
            do {
                Point p = fhit->vertex()->point();
                glVertex3f(p[0], p[1], p[2]);
            } while (++fhit != end);
#endif

            glEnd();
        }
    }

    glDisable(GL_LIGHTING);
    glDisable(GL_LIGHT0);
    glDisable(GL_NORMALIZE);

    if (HalfedgesOn) {
        glLineWidth(2.0f);
        for (MyMesh::Halfedge_const_iterator hit = currentMesh->halfedges_begin();
             hit != currentMesh->halfedges_end(); ++hit) {
            drawHalfedge(hit);
        }
    }

    if (WireFrameOn) {
        glLineWidth(2.0f);
        glBegin(GL_LINES);
        glColor3f(0.0f, 0.0f, 0.0f);
        for (MyMesh::Halfedge_const_iterator hit = currentMesh->halfedges_begin();
             hit != currentMesh->halfedges_end(); ++hit) {

            if (hit->isOriginal())
                glColor3f(0.0f, 0.0f, 0.0f);
            else if (hit->isAdded())
                glColor3f(0.0f, 0.0f, 1.0f);
            else // to remove
                glColor3f(0.0f, 1.0f, 0.0f);
            Point p = hit->vertex()->point();
            glVertex3f(p[0], p[1], p[2]);
            p = hit->opposite()->vertex()->point();
            glVertex3f(p[0], p[1], p[2]);
        }
        glEnd();
    }

    if (VerticesOn) {
        glPointSize(10.0f);
        glBegin(GL_POINTS);
        for (MyMesh::Vertex_const_iterator vit = currentMesh->vertices_begin();
             vit != currentMesh->vertices_end(); ++vit) {
            if (vit->isConquered())
                glColor3f(1.0f, 0.0f, 0.0f);
            else
                glColor3f(0.0f, 0.0f, 0.0f);
            Point p = vit->point();
            glVertex3f(p[0], p[1], p[2]);
        }
        glEnd();
    }

    glDisable(GL_DEPTH_TEST);

    if (PrintToFileOn) {
        char FileName[256], Command[256];
        sprintf(FileName, "./temp.ppm");
        glPixelStorei(GL_PACK_ALIGNMENT, 1);
#ifdef _WIN32
        glReadPixels(0, 0, WindowW, WindowH, GL_RGB, GL_UNSIGNED_BYTE, Framebuffer);
#else
        glReadPixels( (1024-WindowW)/2, (768-WindowH)/2, WindowW, WindowH, GL_RGB, GL_UNSIGNED_BYTE, Framebuffer );
#endif
        SavePPM(FileName, Framebuffer, WindowW, WindowH);

#ifdef _WIN32
        sprintf(Command, "i_view32.exe temp.ppm /convert=%s%d%d%d%d.jpg", PrintFileName, ((Time / 1000) % 10),
                ((Time / 100) % 10), ((Time / 10) % 10), (Time % 10));
#else
        sprintf(Command, "convert ./temp.ppm ./%s%d%d%d%d.png",PrintFileName,((Time/1000)%10),((Time/100)%10),((Time/10)%10),(Time%10));
#endif
        printf("performing: '%s'\n", Command);
        int i_ret = system(Command);
        Time++;
#if 1
        PrintToFileOn = 0;
#endif
    } else {
        displayMessage();
    }
    glutSwapBuffers();
}


// Print the usage of the executable.
void printUsage() {
    fprintf(stderr, "Usage: ppmc [mode] [options] <filepath>\n"
                    "\n"
                    "Mode:\n"
                    "   -c : compression (default mode if none is specified).\n"
                    "   -d <percentage> : decompression at a given percentage. Use -o option to set the output file name.\n"
                    "   -D <percentage> : same as -d but write all the intermediate levels of details decompressed files.\n"
                    "   -h : display this usage message.\n"
                    "\n"
                    "Options:\n"
                    "   --gui : display the GUI.\n"
                    "   -o <filepath> : set the output file name for the compression or the decompression.\n"
                    "   -q <quantization> : set the number of quantization bits. The default value is 12.\n"
                    "   --enable-adaptive-quantization : enable the adaptive quantization scheme. Should be used with --disable-lifting-scheme.\n"
                    "   --disable-lifting-scheme : disable the lifting scheme.\n"
                    "   --disable-curvature-prediction : disable the curvature prediction.\n"
                    "   --disable-connectivity-prediction-faces : disable the connectivity prediction scheme for the faces.\n"
                    "   --disable-triangle-mesh-connectivity-prediction-faces : disable the connectivity prediction scheme for the faces of the triangular meshes.\n"
                    "   --disable-connectivity-prediction-edges : disable the connectivity prediction scheme for the edges.\n"
                    "   --forbid-concave-faces : forbid during the first part of the compression to generate concave faces.\n");
}


int main(int argc, char **argv) {
    char *psz_filePath = NULL;
    std::string filePathOutput("");
    int i_mode = -1;
    bool b_displayGUI = false;

    // Codec features status.
    bool b_useAdaptiveQuantization = false;
    bool b_useLiftingScheme = true;
    bool b_useCurvaturePrediction = true;
    bool b_useConnectivityPredictionFaces = true;
    bool b_useConnectivityPredictionEdges = true;
    bool b_allowConcaveFaces = true;
    bool b_useTriangleMeshConnectivityPredictionFaces = true;
    unsigned i_quantBit = 12;
    unsigned i_decompPercentage = 100;

    std::cout << "+-------------------------------------------------------------------------+" << std::endl;
    std::cout << "|                    Progressive Polygon Mesh Compression                 |" << std::endl;
    std::cout << "|                                    v0.1                                 |" << std::endl;
    std::cout << "| Adrien Maglo and Clément Courbet - MAS laboratory, Ecole Centrale Paris |" << std::endl;
    std::cout << "+-------------------------------------------------------------------------+" << std::endl;

    // Parse the command line.
    if (argc < 2) {
        printUsage();
        return EXIT_FAILURE;
    } else {
        int i = 1;
        while (i < argc) {
#define EXIT_IF_LAST_ARGUMENT() \
    if (i == argc - 1)          \
    {                           \
        printUsage();           \
        return EXIT_FAILURE;    \
    }

            if (!strcmp(argv[i], "-c")) {
                EXIT_IF_LAST_ARGUMENT()
                i_mode = COMPRESSION_MODE_ID;
            } else if (!strcmp(argv[i], "-d")) {
                if (i >= argc - 2) {
                    printUsage();
                    return EXIT_FAILURE;
                }
                i_decompPercentage = atoi(argv[++i]);
                if (i_decompPercentage < 0 || i_decompPercentage > 100) {
                    printUsage();
                    return EXIT_FAILURE;
                }
                i_mode = DECOMPRESSION_MODE_ID;
            } else if (!strcmp(argv[i], "-D")) {
                if (i >= argc - 2) {
                    printUsage();
                    return EXIT_FAILURE;
                }
                i_decompPercentage = atoi(argv[++i]);
                if (i_decompPercentage < 0 || i_decompPercentage > 100) {
                    printUsage();
                    return EXIT_FAILURE;
                }
                i_mode = DECOMPRESSION_MODE_WRITE_ALL_ID;
            } else if (!strcmp(argv[i], "-h")) {
                printUsage();
                return EXIT_SUCCESS;
            } else if (!strcmp(argv[i], "-o")) {
                if (i >= argc - 2) {
                    printUsage();
                    return EXIT_FAILURE;
                }
                filePathOutput = std::string(argv[++i]);
            } else if (!strcmp(argv[i], "--gui")) {
                EXIT_IF_LAST_ARGUMENT()
                b_displayGUI = true;
            } else if (!strcmp(argv[i], "-q")) {
                if (i >= argc - 2) {
                    printUsage();
                    return EXIT_FAILURE;
                }
                i_quantBit = atoi(argv[++i]);
                if (i_quantBit < 4 || i_quantBit > 16) {
                    printf("Wrong quantization: 3 < Q < 17.\n");
                    printUsage();
                    return EXIT_FAILURE;
                }
            } else if (!strcmp(argv[i], "--enable-adaptive-quantization")) {
                EXIT_IF_LAST_ARGUMENT()
                b_useAdaptiveQuantization = true;
            } else if (!strcmp(argv[i], "--disable-lifting-scheme")) {
                EXIT_IF_LAST_ARGUMENT()
                b_useLiftingScheme = false;
            } else if (!strcmp(argv[i], "--disable-curvature-prediction")) {
                EXIT_IF_LAST_ARGUMENT()
                b_useCurvaturePrediction = false;
            } else if (!strcmp(argv[i], "--disable-connectivity-prediction-faces")) {
                EXIT_IF_LAST_ARGUMENT()
                b_useConnectivityPredictionFaces = false;
            } else if (!strcmp(argv[i], "--disable-triangle-mesh-connectivity-prediction-faces")) {
                EXIT_IF_LAST_ARGUMENT()
                b_useTriangleMeshConnectivityPredictionFaces = false;
            } else if (!strcmp(argv[i], "--disable-connectivity-prediction-edges")) {
                EXIT_IF_LAST_ARGUMENT()
                b_useConnectivityPredictionEdges = false;
            } else if (!strcmp(argv[i], "--forbid-concave-faces")) {
                EXIT_IF_LAST_ARGUMENT()
                b_allowConcaveFaces = false;
            } else if (i == argc - 1) {
                if (i_mode == -1)
                    i_mode = 0;
                psz_filePath = argv[i];
            } else {
                printUsage();
                return EXIT_FAILURE;
            }
            i++;
#undef EXIT_IF_LAST_ARGUMENT
        }
    }

    // Set a correct ouput file path.
    if (i_mode != COMPRESSION_MODE_ID) {
        if (filePathOutput == "")
            filePathOutput = std::string("out");
        if (filePathOutput.find(".off") == filePathOutput.length() - 4)
            filePathOutput = filePathOutput.substr(0, filePathOutput.length() - 4);
    } else {
        if (filePathOutput == "")
            filePathOutput = std::string("out.pp3d");
        else if (filePathOutput.find(".pp3d") != filePathOutput.length() - 5)
            filePathOutput += ".pp3d";
    }

    // Init the random number generator.
    srand(4212);

    //read the mesh:
    currentMesh = new MyMesh(psz_filePath, filePathOutput, i_decompPercentage,
                             i_mode, i_quantBit, b_useAdaptiveQuantization,
                             b_useLiftingScheme, b_useCurvaturePrediction,
                             b_useConnectivityPredictionFaces, b_useConnectivityPredictionEdges,
                             b_allowConcaveFaces, b_useTriangleMeshConnectivityPredictionFaces);

    if (b_displayGUI) {
        // Configure the view.
        f_diag = currentMesh->getBBoxDiagonal();
        DistZ = f_diag * 2;

        Vector meshCenterPoint = currentMesh->getBBoxCenter();
        boundingBoxTranslateX = -meshCenterPoint.x();
        boundingBoxTranslateY = -meshCenterPoint.y();
        boundingBoxTranslateZ = -meshCenterPoint.z();

        camMotionStep = f_diag;

        //TODO: check that the mesh does not have boundaries

        glutInit(&argc, argv);

        glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH | GLUT_MULTISAMPLE);
        glutInitWindowSize(WindowW, WindowH);
        glutInitWindowPosition(0, 0);
        glutCreateWindow("Progressive Polygon Mesh Compressor");

        glEnable(GLUT_MULTISAMPLE);
        glShadeModel(GL_SMOOTH);

        InitLight();

        glutDisplayFunc(myDisplay);
        glutReshapeFunc(myReshape);
        glutIdleFunc(myIdle);

        glutMouseFunc(myMouseFunc);
        glutMotionFunc(myMotionFunc);
        glutKeyboardFunc(myKeyboard);

        glutMainLoop();
    } else {
        // Run the complete job and exit.
        currentMesh->completeOperation();
    }

    return EXIT_SUCCESS;
}

