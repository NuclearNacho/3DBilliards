#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>
#include <stdexcept>
#include <fstream>
#include <sstream>
#include <iostream>

#include "resource_manager.h"

namespace game {

ResourceManager::ResourceManager(void){
}


ResourceManager::~ResourceManager(){
}


void ResourceManager::AddResource(ResourceType type, const std::string name, GLuint resource, GLsizei size){

    Resource *res;

    res = new Resource(type, name, resource, size);

    resource_.push_back(res);
}


void ResourceManager::AddResource(ResourceType type, const std::string name, GLuint array_buffer, GLuint element_array_buffer, GLsizei size){

    Resource *res;

    res = new Resource(type, name, array_buffer, element_array_buffer, size);

    resource_.push_back(res);
}


void ResourceManager::LoadResource(ResourceType type, const std::string name, const char *filename){

    // Call appropriate method depending on type of resource
    if (type == Material){
        LoadMaterial(name, filename);
    } else {
        throw(std::invalid_argument(std::string("Invalid type of resource")));
    }
}


Resource *ResourceManager::GetResource(const std::string name) const {

    // Find resource with the specified name
    for (int i = 0; i < resource_.size(); i++){
        if (resource_[i]->GetName() == name){
            return resource_[i];
        }
    }
    return NULL;
}


void ResourceManager::LoadMaterial(const std::string name, const char *prefix){

    // Load vertex program source code
    std::string filename = std::string(prefix) + std::string(VERTEX_PROGRAM_EXTENSION);
    std::string vp = LoadTextFile(filename.c_str());

    // Load fragment program source code
    filename = std::string(prefix) + std::string(FRAGMENT_PROGRAM_EXTENSION);
    std::string fp = LoadTextFile(filename.c_str());

    // Create a shader from the vertex program source code
    GLuint vs = glCreateShader(GL_VERTEX_SHADER);
    const char *source_vp = vp.c_str();
    glShaderSource(vs, 1, &source_vp, NULL);
    glCompileShader(vs);

    // Check if shader compiled successfully
    GLint status;
    glGetShaderiv(vs, GL_COMPILE_STATUS, &status);
    if (status != GL_TRUE){
        char buffer[512];
        glGetShaderInfoLog(vs, 512, NULL, buffer);
        throw(std::ios_base::failure(std::string("Error compiling vertex shader: ")+std::string(buffer)));
    }

    // Create a shader from the fragment program source code
    GLuint fs = glCreateShader(GL_FRAGMENT_SHADER);
    const char *source_fp = fp.c_str();
    glShaderSource(fs, 1, &source_fp, NULL);
    glCompileShader(fs);

    // Check if shader compiled successfully
    glGetShaderiv(fs, GL_COMPILE_STATUS, &status);
    if (status != GL_TRUE){
        char buffer[512];
        glGetShaderInfoLog(fs, 512, NULL, buffer);
        throw(std::ios_base::failure(std::string("Error compiling fragment shader: ")+std::string(buffer)));
    }

    // Create a shader program linking both vertex and fragment shaders
    // together
    GLuint sp = glCreateProgram();
    glAttachShader(sp, vs);
    glAttachShader(sp, fs);
    glLinkProgram(sp);

    // Check if shaders were linked successfully
    glGetProgramiv(sp, GL_LINK_STATUS, &status);
    if (status != GL_TRUE){
        char buffer[512];
        glGetProgramInfoLog(sp, 512, NULL, buffer);
        throw(std::ios_base::failure(std::string("Error linking shaders: ")+std::string(buffer)));
    }

    // Delete memory used by shaders, since they were already compiled
    // and linked
    glDeleteShader(vs);
    glDeleteShader(fs);

    // Add a resource for the shader program
    AddResource(Material, name, sp, 0);
}


std::string ResourceManager::LoadTextFile(const char *filename){

    // Open file
    std::ifstream f;
    f.open(filename);
    if (f.fail()){
        throw(std::ios_base::failure(std::string("Error opening file ")+std::string(filename)));
    }

    // Read file
    std::string content;
    std::string line;
    while(std::getline(f, line)){
        content += line + "\n";
    }

    // Close file
    f.close();

    return content;
}


void ResourceManager::CreateSphere(std::string object_name, float radius, int num_samples_theta, int num_samples_phi){

    // Create a sphere using a well-known parameterization

    // Number of vertices and faces to be created
    const GLuint vertex_num = num_samples_theta*num_samples_phi;
    const GLuint face_num = num_samples_theta*(num_samples_phi-1)*2;

    // Number of attributes for vertices and faces
    const int vertex_att = 11;
    const int face_att = 3;

    // Data buffers 
    GLfloat *vertex = NULL;
    GLuint *face = NULL;

    // Allocate memory for buffers
    try {
        vertex = new GLfloat[vertex_num * vertex_att]; // 11 attributes per vertex: 3D position (3), 3D normal (3), RGB color (3), 2D texture coordinates (2)
        face = new GLuint[face_num * face_att]; // 3 indices per face
    }
    catch  (std::exception &e){
        throw e;
    }

    // Create vertices 
    float theta, phi; // Angles for parametric equation
    glm::vec3 vertex_position;
    glm::vec3 vertex_normal;
    glm::vec3 vertex_color;
    glm::vec2 vertex_coord;
   
    for (int i = 0; i < num_samples_theta; i++){
            
        theta = 2.0*glm::pi<GLfloat>()*i/(num_samples_theta-1); // angle theta
            
        for (int j = 0; j < num_samples_phi; j++){
                    
            phi = glm::pi<GLfloat>()*j/(num_samples_phi-1); // angle phi

            // Define position, normal and color of vertex
            vertex_normal = glm::vec3(cos(theta)*sin(phi), sin(theta)*sin(phi), -cos(phi));
            // We need z = -cos(phi) to make sure that the z coordinate runs from -1 to 1 as phi runs from 0 to pi
            // Otherwise, the normal will be inverted
            vertex_position = glm::vec3(vertex_normal.x*radius, 
                                        vertex_normal.y*radius, 
                                        vertex_normal.z*radius),
            vertex_color = glm::vec3(((float)i)/((float)num_samples_theta), 1.0-((float)j)/((float)num_samples_phi), ((float)j)/((float)num_samples_phi));
            vertex_coord = glm::vec2(((float)i)/((float)num_samples_theta), 1.0-((float)j)/((float)num_samples_phi));

            // Add vectors to the data buffer
            for (int k = 0; k < 3; k++){
                vertex[(i*num_samples_phi+j)*vertex_att + k] = vertex_position[k];
                vertex[(i*num_samples_phi+j)*vertex_att + k + 3] = vertex_normal[k];
                vertex[(i*num_samples_phi+j)*vertex_att + k + 6] = vertex_color[k];
            }
            vertex[(i*num_samples_phi+j)*vertex_att + 9] = vertex_coord[0];
            vertex[(i*num_samples_phi+j)*vertex_att + 10] = vertex_coord[1];
        }
    }

    // Create faces
    for (int i = 0; i < num_samples_theta; i++){
        for (int j = 0; j < (num_samples_phi-1); j++){
            // Two triangles per quad
            glm::vec3 t1(((i + 1) % num_samples_theta)*num_samples_phi + j, 
                         i*num_samples_phi + (j + 1),
                         i*num_samples_phi + j);
            glm::vec3 t2(((i + 1) % num_samples_theta)*num_samples_phi + j, 
                         ((i + 1) % num_samples_theta)*num_samples_phi + (j + 1), 
                         i*num_samples_phi + (j + 1));
            // Add two triangles to the data buffer
            for (int k = 0; k < 3; k++){
                face[(i*(num_samples_phi-1)+j)*face_att*2 + k] = (GLuint) t1[k];
                face[(i*(num_samples_phi-1)+j)*face_att*2 + k + face_att] = (GLuint) t2[k];
            }
        }
    }

    // Create OpenGL buffers and copy data
    //GLuint vao;
    //glGenVertexArrays(1, &vao);
    //glBindVertexArray(vao);

    GLuint vbo, ebo;
    glGenBuffers(1, &vbo);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, vertex_num * vertex_att * sizeof(GLfloat), vertex, GL_STATIC_DRAW);

    glGenBuffers(1, &ebo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, face_num * face_att * sizeof(GLuint), face, GL_STATIC_DRAW);

    // Free data buffers
    delete [] vertex;
    delete [] face;

    // Create resource
    AddResource(Mesh, object_name, vbo, ebo, face_num * face_att);
}

void ResourceManager::CreateColoredSphere(std::string object_name, const glm::vec3 &color, bool gradient_to_white, float radius, int num_samples_theta, int num_samples_phi) {

    // Create a sphere using a parametric parameterization, identical topology to CreateSphere
    const GLuint vertex_num = num_samples_theta * num_samples_phi;
    const GLuint face_num = num_samples_theta * (num_samples_phi - 1) * 2;
    const int vertex_att = 11;
    const int face_att = 3;

    GLfloat *vertex = NULL;
    GLuint *face = NULL;

    try {
        vertex = new GLfloat[vertex_num * vertex_att];
        face = new GLuint[face_num * face_att];
    }
    catch (std::exception &e) {
        throw e;
    }

    float theta, phi;
    glm::vec3 vertex_position;
    glm::vec3 vertex_normal;
    glm::vec3 vertex_color;
    glm::vec2 vertex_coord;

    for (int i = 0; i < num_samples_theta; i++) {
        theta = 2.0f * glm::pi<GLfloat>() * i / (num_samples_theta - 1);
        for (int j = 0; j < num_samples_phi; j++) {
            phi = glm::pi<GLfloat>() * j / (num_samples_phi - 1);

            vertex_normal = glm::vec3(cos(theta) * sin(phi), sin(theta) * sin(phi), -cos(phi));
            vertex_position = glm::vec3(vertex_normal.x * radius, vertex_normal.y * radius, vertex_normal.z * radius);

            // Choose color: either solid 'color' or interpolated towards white
            if (!gradient_to_white) {
                vertex_color = color;
            } else {
                // Interpolate toward white based on the hemisphere: more white near +Y surface
                // t in [0,1], compute from normal.y so poles interpolate more to white
                float t = glm::clamp(0.5f * (1.0f - vertex_normal.y), 0.0f, 1.0f);
                vertex_color = (1.0f - t) * color + t * glm::vec3(1.0f);
            }

            vertex_coord = glm::vec2(((float)i) / ((float)num_samples_theta), 1.0f - ((float)j) / ((float)num_samples_phi));

            for (int k = 0; k < 3; k++) {
                vertex[(i * num_samples_phi + j) * vertex_att + k] = vertex_position[k];
                vertex[(i * num_samples_phi + j) * vertex_att + k + 3] = vertex_normal[k];
                vertex[(i * num_samples_phi + j) * vertex_att + k + 6] = vertex_color[k];
            }
            vertex[(i * num_samples_phi + j) * vertex_att + 9] = vertex_coord[0];
            vertex[(i * num_samples_phi + j) * vertex_att + 10] = vertex_coord[1];
        }
    }

    // Create faces (same as CreateSphere)
    for (int i = 0; i < num_samples_theta; i++) {
        for (int j = 0; j < (num_samples_phi - 1); j++) {
            glm::vec3 t1(((i + 1) % num_samples_theta) * num_samples_phi + j,
                         i * num_samples_phi + (j + 1),
                         i * num_samples_phi + j);
            glm::vec3 t2(((i + 1) % num_samples_theta) * num_samples_phi + j,
                         ((i + 1) % num_samples_theta) * num_samples_phi + (j + 1),
                         i * num_samples_phi + (j + 1));
            for (int k = 0; k < 3; k++) {
                face[(i * (num_samples_phi - 1) + j) * face_att * 2 + k] = (GLuint)t1[k];
                face[(i * (num_samples_phi - 1) + j) * face_att * 2 + k + face_att] = (GLuint)t2[k];
            }
        }
    }

    // Upload to GL
    GLuint vbo, ebo;
    glGenBuffers(1, &vbo);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, vertex_num * vertex_att * sizeof(GLfloat), vertex, GL_STATIC_DRAW);

    glGenBuffers(1, &ebo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, face_num * face_att * sizeof(GLuint), face, GL_STATIC_DRAW);

    delete[] vertex;
    delete[] face;

    AddResource(Mesh, object_name, vbo, ebo, face_num * face_att);
}

void ResourceManager::CreateTorus(std::string object_name, float loop_radius, float circle_radius, int num_loop_samples, int num_circle_samples) {

    // Increase torus loop radius by 50% to make diameter 50% larger
    float loopR = loop_radius * 1.5f;
    float tubeR = circle_radius;

    // Number of vertices and faces
    const GLuint vertex_num = num_loop_samples * num_circle_samples;
    const GLuint face_num = num_loop_samples * num_circle_samples * 2;

    // Attributes: position(3), normal(3), color(3), texcoord(2)
    const int vertex_att = 11;
    const int face_att = 3;

    GLfloat* vertex = nullptr;
    GLuint* face = nullptr;

    try {
        vertex = new GLfloat[vertex_num * vertex_att];
        face = new GLuint[face_num * face_att];
    }
    catch (std::exception &e) {
        throw e;
    }

    // Build vertices
    for (int i = 0; i < num_loop_samples; ++i) {
        float u = 2.0f * glm::pi<float>() * i / num_loop_samples; // loop angle
        float cu = cos(u), su = sin(u);
        for (int j = 0; j < num_circle_samples; ++j) {
            float v = 2.0f * glm::pi<float>() * j / num_circle_samples; // tube angle
            float cv = cos(v), sv = sin(v);

            // Position using scaled loopR and tubeR
            float x = (loopR + tubeR * cv) * cu;
            float y = (loopR + tubeR * cv) * su;
            float z = tubeR * sv;

            // Normal: vector from tube center to surface point (normalized)
            glm::vec3 n = glm::normalize(glm::vec3(cu * cv, su * cv, sv));

            // Color: slightly darker grey for guides
            glm::vec3 color = glm::vec3(0.3f, 0.3f, 0.3f);

            // Texcoords
            float s = (float)i / (float)num_loop_samples;
            float t = (float)j / (float)num_circle_samples;

            int idx = (i * num_circle_samples + j) * vertex_att;
            vertex[idx + 0] = x;
            vertex[idx + 1] = y;
            vertex[idx + 2] = z;
            vertex[idx + 3] = n.x;
            vertex[idx + 4] = n.y;
            vertex[idx + 5] = n.z;
            vertex[idx + 6] = color.x;
            vertex[idx + 7] = color.y;
            vertex[idx + 8] = color.z;
            vertex[idx + 9] = s;
            vertex[idx + 10] = t;
        }
    }

    // Build faces (two triangles per quad, wrapping on both dimensions)
    int fidx = 0;
    for (int i = 0; i < num_loop_samples; ++i) {
        int inext = (i + 1) % num_loop_samples;
        for (int j = 0; j < num_circle_samples; ++j) {
            int jnext = (j + 1) % num_circle_samples;

            // indices of quad corners
            GLuint a = i * num_circle_samples + j;
            GLuint b = inext * num_circle_samples + j;
            GLuint c = inext * num_circle_samples + jnext;
            GLuint d = i * num_circle_samples + jnext;

            // triangle 1: a, b, d
            face[fidx * face_att + 0] = a;
            face[fidx * face_att + 1] = b;
            face[fidx * face_att + 2] = d;
            ++fidx;

            // triangle 2: b, c, d
            face[fidx * face_att + 0] = b;
            face[fidx * face_att + 1] = c;
            face[fidx * face_att + 2] = d;
            ++fidx;
        }
    }

    // Upload to GL buffers
    GLuint vbo, ebo;
    glGenBuffers(1, &vbo);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, vertex_num * vertex_att * sizeof(GLfloat), vertex, GL_STATIC_DRAW);

    glGenBuffers(1, &ebo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, face_num * face_att * sizeof(GLuint), face, GL_STATIC_DRAW);

    // Free CPU memory
    delete[] vertex;
    delete[] face;

    // Register resource
    AddResource(Mesh, object_name, vbo, ebo, face_num * face_att);
}

void ResourceManager::CreateBox(std::string object_name, float width, float height, float depth) {

    // Box centered at origin, depth along +Z. We'll create 8 vertices and 12 triangles.
    const int vertex_att = 11;
    const int face_att = 3;

    const GLuint vertex_num = 8;
    const GLuint face_num = 12;

    GLfloat *vertex = nullptr;
    GLuint *face = nullptr;

    try {
        vertex = new GLfloat[vertex_num * vertex_att];
        face = new GLuint[face_num * face_att];
    } catch (std::exception &e) {
        throw e;
    }

    float hx = width * 0.5f;
    float hy = height * 0.5f;
    float hz = depth * 0.5f;

    // Define 8 corner positions
    glm::vec3 positions[8] = {
        glm::vec3(-hx, -hy, -hz),
        glm::vec3( hx, -hy, -hz),
        glm::vec3( hx,  hy, -hz),
        glm::vec3(-hx,  hy, -hz),
        glm::vec3(-hx, -hy,  hz),
        glm::vec3( hx, -hy,  hz),
        glm::vec3( hx,  hy,  hz),
        glm::vec3(-hx,  hy,  hz)
    };

    // Per-vertex normals (we'll assign averaged normals per face corners to keep it simple)
    glm::vec3 normals[8] = {
        glm::vec3(-1,-1,-1),
        glm::vec3( 1,-1,-1),
        glm::vec3( 1, 1,-1),
        glm::vec3(-1, 1,-1),
        glm::vec3(-1,-1, 1),
        glm::vec3( 1,-1, 1),
        glm::vec3( 1, 1, 1),
        glm::vec3(-1, 1, 1)
    };
    for (int i = 0; i < 8; ++i) normals[i] = glm::normalize(normals[i]);

    // Color: white (alpha handled by shader if supported)
    glm::vec3 color(1.0f, 1.0f, 1.0f);

    // Texcoords (not important)
    glm::vec2 uvs[8] = {
        glm::vec2(0.0f,0.0f), glm::vec2(1.0f,0.0f), glm::vec2(1.0f,1.0f), glm::vec2(0.0f,1.0f),
        glm::vec2(0.0f,0.0f), glm::vec2(1.0f,0.0f), glm::vec2(1.0f,1.0f), glm::vec2(0.0f,1.0f)
    };

    // Fill vertex buffer (position, normal, color, uv)
    for (int i = 0; i < 8; ++i) {
        int idx = i * vertex_att;
        vertex[idx + 0] = positions[i].x;
        vertex[idx + 1] = positions[i].y;
        vertex[idx + 2] = positions[i].z;
        vertex[idx + 3] = normals[i].x;
        vertex[idx + 4] = normals[i].y;
        vertex[idx + 5] = normals[i].z;
        vertex[idx + 6] = color.x;
        vertex[idx + 7] = color.y;
        vertex[idx + 8] = color.z;
        vertex[idx + 9] = uvs[i].x;
        vertex[idx + 10] = uvs[i].y;
    }

    // Index order (12 triangles)
    GLuint inds[36] = {
        // back face (-Z)
        0,1,2, 0,2,3,
        // front face (+Z)
        4,6,5, 4,7,6,
        // left face (-X)
        0,3,7, 0,7,4,
        // right face (+X)
        1,5,6, 1,6,2,
        // bottom face (-Y)
        0,4,5, 0,5,1,
        // top face (+Y)
        3,2,6, 3,6,7
    };

    for (int i = 0; i < face_num * face_att; ++i) face[i] = inds[i];

    // Upload to GL
    GLuint vbo, ebo;
    glGenBuffers(1, &vbo);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, vertex_num * vertex_att * sizeof(GLfloat), vertex, GL_STATIC_DRAW);

    glGenBuffers(1, &ebo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, face_num * face_att * sizeof(GLuint), face, GL_STATIC_DRAW);

    delete[] vertex;
    delete[] face;

    AddResource(Mesh, object_name, vbo, ebo, face_num * face_att);
}

} // namespace game;
