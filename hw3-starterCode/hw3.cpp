/* **************************
 * CSCI 420
 * Assignment 3 Raytracer
 * Name: Menaka Ravi
 * *************************
*/

#ifdef WIN32
#include <windows.h>
#endif

#if defined(WIN32) || defined(linux)
#include <GL/gl.h>
#include <GL/glut.h>
#elif defined(__APPLE__)
#include <OpenGL/gl.h>
#include <GLUT/glut.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <cmath>
#include <vector>
#ifdef WIN32
#define strcasecmp _stricmp
#endif

#include <imageIO.h>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>

#define MAX_TRIANGLES 20000
#define MAX_SPHERES 100
#define MAX_LIGHTS 100

char * filename = NULL;

//different display modes
#define MODE_DISPLAY 1
#define MODE_JPEG 2

int mode = MODE_DISPLAY;
bool antialiasing = true;

//you may want to make these smaller for debugging purposes
#define WIDTH 640
#define HEIGHT 480

//the field of view of the camera
#define fov 60.0

const double ASPECT_RATIO = (double)WIDTH / HEIGHT;
const double FOV_FACTOR = tan((fov / 2.0) * (3.1415926535 / 180.0));

unsigned char buffer[HEIGHT][WIDTH][3];

struct Vertex
{
	double position[3];
	double color_diffuse[3];
	double color_specular[3];
	double normal[3];
	double shininess;
};

struct Triangle
{
	Vertex v[3];
};

struct Sphere
{
	double position[3];
	double color_diffuse[3];
	double color_specular[3];
	double shininess;
	double radius;
};

struct Light
{
	double position[3];
	double color[3];
};

Triangle triangles[MAX_TRIANGLES];
Sphere spheres[MAX_SPHERES];
Light lights[MAX_LIGHTS];
double ambient_light[3];

int num_triangles = 0;
int num_spheres = 0;
int num_lights = 0;

void plot_pixel_display(int x, int y, unsigned char r, unsigned char g, unsigned char b);
void plot_pixel_jpeg(int x, int y, unsigned char r, unsigned char g, unsigned char b);
void plot_pixel(int x, int y, unsigned char r, unsigned char g, unsigned char b);

glm::highp_dvec3 clampColor(glm::highp_dvec3 color)
{
	if (color.r < 0.0)
		color.r = 0.0;
	if (color.g < 0.0)
		color.g = 0.0;
	if (color.b < 0.0)
		color.b = 0.0;
	if (color.r > 1.0)
		color.r = 1.0;
	if (color.g > 1.0)
		color.g = 1.0;
	if (color.b > 1.0)
		color.b = 1.0;

	return color;
}

class Ray
{
	glm::highp_dvec3 pos;
	glm::highp_dvec3 dir;

public:
	Ray(glm::highp_dvec3 position, glm::highp_dvec3 direction)
	{
		pos = position;
		dir = direction;
	}

	// check if ray intersects with triangle
	bool triangleIntersect(const Triangle & triangle, glm::highp_dvec3 & intersection)
	{
		glm::highp_dvec3 v0 = { triangle.v[0].position[0], triangle.v[0].position[1], triangle.v[0].position[2] };
		glm::highp_dvec3 v1 = { triangle.v[1].position[0], triangle.v[1].position[1], triangle.v[1].position[2] };
		glm::highp_dvec3 v2 = { triangle.v[2].position[0], triangle.v[2].position[1], triangle.v[2].position[2] };

		// intersection with plane of triangle
		// find plane normal
		glm::highp_dvec3 normal = glm::cross((v1 - v0), (v2 - v0));
		normal = glm::normalize(normal);

		double denom = (glm::dot(normal, dir));
		if (fabs(denom) < 1e-10)
			return false;

		double t = (glm::dot(normal, v0 - pos)) / denom;
		if (t <= 1e-10)
			return false;

		intersection = pos + (dir * t);

		// inside outside test
		// calculate barycentric coordinates in 3D
		glm::highp_dvec3 area1 = glm::cross((v1 - v0), (intersection - v0));
		if (glm::dot(area1, normal) < 0)
			return false;

		glm::highp_dvec3 area2 = glm::cross((v2 - v1), (intersection - v1));
		if (glm::dot(area2, normal) < 0)
			return false;

		glm::highp_dvec3 area3 = glm::cross((v0 - v2), (intersection - v2));
		if (glm::dot(area3, normal) < 0)
			return false;

		return true;
	}

	// check if ray intersects with sphere
	bool sphereIntersect(const Sphere & sphere, glm::highp_dvec3 & intersection)
	{
		double b, c;
		glm::highp_dvec3 center = { sphere.position[0], sphere.position[1], sphere.position[2] };
		b = 2.0f * glm::dot(dir, (pos - center));
		c = pow(pos.x - center.x, 2) + pow(pos.y - center.y, 2) + pow(pos.z - center.z, 2) - pow(sphere.radius, 2);

		// find value under sqrt
		double root = b * b - 4 * c;

		if (root < 0)
			return false;

		double t0, t1;
		if (root == 0)
			t0 = t1 = -b / 2.0;
		else
		{
			t0 = (-b + sqrt(root)) / 2;
			t1 = (-b - sqrt(root)) / 2;
		}

		// assign smallest positive t-value to t0
		if (t0 < 0 && t1 < 0)
			return false;

		if (t1 < t0 && t1 > 0)
			t0 = t1;
		else if (t0 < 0)
			t0 = t1;

		intersection = pos + (dir * t0);
		return true;
	}
};

// send a ray from camera to each pixel
Ray cameraRay(double _x, double _y)
{
	glm::highp_dvec3 startPt = { 0.0, 0.0, 0.0 };

	double x, y;
	// normalized device coordinates
	x = (_x + 0.5) / WIDTH;
	y = (_y + 0.5) / HEIGHT;

	// screen coordinates
	x = 2 * x - 1;
	y = 2 * y - 1;

	// camera coordinates
	x = x * ASPECT_RATIO * FOV_FACTOR;
	y = y * FOV_FACTOR;

	glm::highp_dvec3 endPt = { x, y, -1.0 };
	glm::highp_dvec3 dir = glm::normalize(endPt);

	return Ray(startPt, dir);
}

// send 5 rays from camera to each pixel for antialiasing
std::vector<Ray> cameraRaysAA(double _x, double _y)
{
	std::vector<Ray> AARays;
	double x, y;
	glm::highp_dvec3 startPt = { 0.0, 0.0, 0.0 };

	// ray0
	// normalized device coordinates
	x = (_x + 0.25) / WIDTH;
	y = (_y + 0.25) / HEIGHT;

	// screen coordinates
	x = 2 * x - 1;
	y = 2 * y - 1;

	// camera coordinates
	x = x * ASPECT_RATIO * FOV_FACTOR;
	y = y * FOV_FACTOR;

	glm::highp_dvec3 endPt0 = { x, y, -1.0 };
	glm::highp_dvec3 dir0 = glm::normalize(endPt0);
	Ray ray0(startPt, dir0);
	AARays.push_back(ray0);

	// ray1
	// normalized device coordinates
	x = (_x + 0.25) / WIDTH;
	y = (_y + 0.75) / HEIGHT;

	// screen coordinates
	x = 2 * x - 1;
	y = 2 * y - 1;

	// camera coordinates
	x = x * ASPECT_RATIO * FOV_FACTOR;
	y = y * FOV_FACTOR;

	glm::highp_dvec3 endPt1 = { x, y, -1.0 };
	glm::highp_dvec3 dir1 = glm::normalize(endPt1);
	Ray ray1(startPt, dir1);
	AARays.push_back(ray1);

	// ray2
	// normalized device coordinates
	x = (_x + 0.75) / WIDTH;
	y = (_y + 0.25) / HEIGHT;

	// screen coordinates
	x = 2 * x - 1;
	y = 2 * y - 1;

	// camera coordinates
	x = x * ASPECT_RATIO * FOV_FACTOR;
	y = y * FOV_FACTOR;

	glm::highp_dvec3 endPt2 = { x, y, -1.0 };
	glm::highp_dvec3 dir2 = glm::normalize(endPt2);
	Ray ray2(startPt, dir2);
	AARays.push_back(ray2);

	// ray3
	// normalized device coordinates
	x = (_x + 0.75) / WIDTH;
	y = (_y + 0.75) / HEIGHT;

	// screen coordinates
	x = 2 * x - 1;
	y = 2 * y - 1;

	// camera coordinates
	x = x * ASPECT_RATIO * FOV_FACTOR;
	y = y * FOV_FACTOR;

	glm::highp_dvec3 endPt3 = { x, y, -1.0 };
	glm::highp_dvec3 dir3 = glm::normalize(endPt3);
	Ray ray3(startPt, dir3);
	AARays.push_back(ray3);

	// ray4
	// normalized device coordinates
	x = (_x + 0.5) / WIDTH;
	y = (_y + 0.5) / HEIGHT;

	// screen coordinates
	x = 2 * x - 1;
	y = 2 * y - 1;

	// camera coordinates
	x = x * ASPECT_RATIO * FOV_FACTOR;
	y = y * FOV_FACTOR;

	glm::highp_dvec3 endPt4 = { x, y, -1.0 };
	glm::highp_dvec3 dir4 = glm::normalize(endPt4);
	Ray ray4(startPt, dir4);
	AARays.push_back(ray4);

	return AARays;
}

// apply Phong shading to triangle at ray intersection
glm::highp_dvec3 trianglePhong(Triangle triangle, glm::highp_dvec3 intersection, Light light)
{
	// triangle vertices
	glm::highp_dvec3 v0 = { triangle.v[0].position[0], triangle.v[0].position[1], triangle.v[0].position[2] };
	glm::highp_dvec3 v1 = { triangle.v[1].position[0], triangle.v[1].position[1], triangle.v[1].position[2] };
	glm::highp_dvec3 v2 = { triangle.v[2].position[0], triangle.v[2].position[1], triangle.v[2].position[2] };

	// vertex normals
	glm::highp_dvec3 n0 = { triangle.v[0].normal[0], triangle.v[0].normal[1], triangle.v[0].normal[2] };
	glm::highp_dvec3 n1 = { triangle.v[1].normal[0], triangle.v[1].normal[1], triangle.v[1].normal[2] };
	glm::highp_dvec3 n2 = { triangle.v[2].normal[0], triangle.v[2].normal[1], triangle.v[2].normal[2] };

	// barycentric coordinates
	/*glm::highp_dvec3 faceNormal = glm::cross((v1 - v0), (v2 - v0));
	double denom = glm::dot(faceNormal, faceNormal);
	double alpha = glm::dot(faceNormal, glm::cross((v2 - v1), (intersection - v1))) / denom;
	double beta = glm::dot(faceNormal, glm::cross((v0 - v2), (intersection - v2))) / denom;
	double gamma = 1.0 - alpha - beta;*/

	double area = glm::length(glm::cross((v1 - v0), (v2 - v0)));
	double alpha = glm::length(glm::cross((v2 - v1), (intersection - v1))) / area;
	double beta = glm::length(glm::cross((v0 - v2), (intersection - v2))) / area;
	double gamma = 1.0 - alpha - beta;

	// interpolate normal from vertex normals
	glm::highp_dvec3 normal = { alpha * n0.x + beta * n1.x + gamma * n2.x,
								alpha * n0.y + beta * n1.y + gamma * n2.y,
								alpha * n0.z + beta * n1.z + gamma * n2.z };
	normal = glm::normalize(normal);

	// interpolate material properties
	glm::highp_dvec3 kd = { alpha * triangle.v[0].color_diffuse[0] + beta * triangle.v[1].color_diffuse[0] + gamma * triangle.v[2].color_diffuse[0],
							alpha * triangle.v[0].color_diffuse[1] + beta * triangle.v[1].color_diffuse[1] + gamma * triangle.v[2].color_diffuse[1],
							alpha * triangle.v[0].color_diffuse[2] + beta * triangle.v[1].color_diffuse[2] + gamma * triangle.v[2].color_diffuse[2] };

	glm::highp_dvec3 ks = { alpha * triangle.v[0].color_specular[0] + beta * triangle.v[1].color_specular[0] + gamma * triangle.v[2].color_specular[0],
							alpha * triangle.v[0].color_specular[1] + beta * triangle.v[1].color_specular[1] + gamma * triangle.v[2].color_specular[1],
							alpha * triangle.v[0].color_specular[2] + beta * triangle.v[1].color_specular[2] + gamma * triangle.v[2].color_specular[2] };

	double shiny = alpha * triangle.v[0].shininess + beta * triangle.v[1].shininess + gamma * triangle.v[2].shininess;

	// light vectors
	glm::highp_dvec3 lightPosition = { light.position[0], light.position[1], light.position[2] };
	glm::highp_dvec3 lightColor = { light.color[0], light.color[1], light.color[2] };
	glm::highp_dvec3 l = glm::normalize(lightPosition - intersection);

	double ldotn = glm::dot(l, normal);
	if (ldotn < 0.0)
		ldotn = 0.0;

	// reflection vector
	glm::highp_dvec3 r = 2 * ldotn * normal - l;
	//glm::highp_dvec3 r = -glm::reflect(l, normal);
	r = glm::normalize(r);

	// camera vector
	glm::highp_dvec3 v = -intersection;
	v = glm::normalize(v);

	double rdotv = glm::dot(r, v);
	if (rdotv < 0.0)
		rdotv = 0.0;

	// final color
	glm::highp_dvec3 color = lightColor * (kd * ldotn + (ks * pow(rdotv, shiny)));
	return color;
}

// apply Phong shading to sphere at ray intersection
glm::highp_dvec3 spherePhong(Sphere sphere, glm::highp_dvec3 intersection, Light light)
{
	// material properties
	glm::highp_dvec3 kd = { sphere.color_diffuse[0], sphere.color_diffuse[1], sphere.color_diffuse[2] };
	glm::highp_dvec3 ks = { sphere.color_specular[0], sphere.color_specular[1], sphere.color_specular[2] };
	double shiny = sphere.shininess;

	// normal vector
	glm::highp_dvec3 center = { sphere.position[0], sphere.position[1], sphere.position[2] };
	glm::highp_dvec3 normal = intersection - center;
	normal = glm::normalize(normal);

	// light vectors
	glm::highp_dvec3 lightPosition = { light.position[0], light.position[1], light.position[2] };
	glm::highp_dvec3 lightColor = { light.color[0], light.color[1], light.color[2] };
	glm::highp_dvec3 l = lightPosition - intersection;
	l = glm::normalize(l);

	double ldotn = glm::dot(l, normal);
	if (ldotn < 0.0)
		ldotn = 0.0;

	// reflection vector
	glm::highp_dvec3 r = (2.0f * ldotn * normal) - l;
	r = glm::normalize(r);

	// camera vector
	glm::highp_dvec3 v = -intersection;
	v = glm::normalize(v);

	double rdotv = glm::dot(r, v);
	if (rdotv < 0.0)
		rdotv = 0.0;

	// final color
	glm::highp_dvec3 color = lightColor * (kd * ldotn + (ks * pow(rdotv, shiny)));
	return color;
}

// calculate color at every pixel
glm::highp_dvec3 finalColor(Ray ray)
{
	glm::highp_dvec3 color = { 1.0, 1.0, 1.0 };

	double closestZ = -1e10;

	// check intersection of ray with every triangle
	for (int i = 0; i < num_triangles; i++)
	{
		glm::highp_dvec3 intersection = { 0, 0, -1e-10 };

		if (ray.triangleIntersect(triangles[i], intersection) && (intersection.z > closestZ))
		{
			closestZ = intersection.z;
			color = glm::highp_dvec3(0.0, 0.0, 0.0);

			// check if triangle is in shadow at ray intersection
			for (int j = 0; j < num_lights; j++)
			{
				bool isInShadow = false;

				glm::highp_dvec3 lightPosition = { lights[j].position[0], lights[j].position[1], lights[j].position[2] };

				glm::highp_dvec3 direction = lightPosition - intersection;
				Ray shadow(intersection, glm::normalize(direction));

				// check for shadow by other spheres
				for (int k = 0; k < num_spheres; k++)
				{
					glm::highp_dvec3 obstruction = { 0, 0, -1e-10 };

					if (shadow.sphereIntersect(spheres[k], obstruction))
					{
						if (glm::length(lightPosition - intersection) > glm::length(obstruction - intersection))
						{
							isInShadow = true;
							break;
						}
					}
				}

				// check for shadow by other triangles
				for (int k = 0; k < num_triangles; k++)
				{
					glm::highp_dvec3 obstruction = { 0, 0, -1e-10 };

					if (k != i && shadow.triangleIntersect(triangles[k], obstruction))
					{
						if (glm::length(lightPosition - intersection) > glm::length(obstruction - intersection))
						{
							isInShadow = true;
							break;
						}
					}
				}

				// if not in shadow, calculate color based on Phong lighting
				if (!isInShadow)
				{
					color += trianglePhong(triangles[i], intersection, lights[j]);
					color = clampColor(color);
				}
			}
		}
	}

	// check intersection of ray with every sphere
	for (int i = 0; i < num_spheres; i++)
	{
		glm::highp_dvec3 intersection = { 0, 0, -1e-10 };

		if (ray.sphereIntersect(spheres[i], intersection) && intersection.z > closestZ)
		{
			closestZ = intersection.z;
			color = glm::highp_dvec3(0.0, 0.0, 0.0);

			// check if sphere is in shadow at ray intersection
			for (int j = 0; j < num_lights; j++)
			{
				bool isInShadow = false;

				glm::highp_dvec3 lightPosition = { lights[j].position[0], lights[j].position[1], lights[j].position[2] };

				glm::highp_dvec3 direction = lightPosition - intersection;
				Ray shadow(intersection, glm::normalize(direction));

				// check for shadow by other spheres
				for (int k = 0; k < num_spheres; k++)
				{
					glm::highp_dvec3 obstruction = { 0, 0, -1e-10 };

					if (k != i && shadow.sphereIntersect(spheres[k], obstruction))
					{
						if (glm::length(obstruction - intersection) < glm::length(lightPosition - intersection))
						{
							isInShadow = true;
							break;
						}
					}
				}

				// check for shadow by other triangles
				for (int k = 0; k < num_triangles; k++)
				{
					glm::highp_dvec3 obstruction = { 0, 0, -1e-10 };

					if (shadow.triangleIntersect(triangles[k], obstruction))
					{
						if (glm::length(obstruction - intersection) < glm::length(lightPosition - intersection))
						{
							isInShadow = true;
							break;
						}
					}
				}

				// if not in shadow, calculate color based on Phong lighting
				if (!isInShadow)
				{
					color += spherePhong(spheres[i], intersection, lights[j]);
					color = clampColor(color);
				}
			}
		}
	}

	// add overall ambient light
	glm::highp_dvec3 ambient = { ambient_light[0], ambient_light[1], ambient_light[2] };
	color += ambient;
	color = clampColor(color);

	return color;
}

//MODIFY THIS FUNCTION
void draw_scene()
{
	//a simple test output
	for (unsigned int x = 0; x < WIDTH; x++)
	{
		glPointSize(2.0);
		glBegin(GL_POINTS);
		for (unsigned int y = 0; y < HEIGHT; y++)
		{
			if (!antialiasing)
			{
				Ray ray = cameraRay(x, y);
				glm::highp_dvec3 color = finalColor(ray);
				plot_pixel(x, y, color.r * 255, color.g * 255, color.b * 255);
			}
			else
			{
				glm::highp_dvec3 color;
				std::vector<Ray> AARays = cameraRaysAA(x, y);
				for (int i = 0; i < 5; i++)
				{
					Ray ray = AARays[i];
					glm::highp_dvec3 rayColor = finalColor(ray);
					color += rayColor;
				}
				color /= 5.0;
				plot_pixel(x, y, color.r * 255, color.g * 255, color.b * 255);
			}
		}
		glEnd();
		glFlush();
	}
	printf("Done!\n");
	fflush(stdout);
}

void plot_pixel_display(int x, int y, unsigned char r, unsigned char g, unsigned char b)
{
	glColor3f(((float)r) / 255.0f, ((float)g) / 255.0f, ((float)b) / 255.0f);
	glVertex2i(x, y);
}

void plot_pixel_jpeg(int x, int y, unsigned char r, unsigned char g, unsigned char b)
{
	buffer[y][x][0] = r;
	buffer[y][x][1] = g;
	buffer[y][x][2] = b;
}

void plot_pixel(int x, int y, unsigned char r, unsigned char g, unsigned char b)
{
	plot_pixel_display(x, y, r, g, b);
	if (mode == MODE_JPEG)
		plot_pixel_jpeg(x, y, r, g, b);
}

void save_jpg()
{
	printf("Saving JPEG file: %s\n", filename);

	ImageIO img(WIDTH, HEIGHT, 3, &buffer[0][0][0]);
	if (img.save(filename, ImageIO::FORMAT_JPEG) != ImageIO::OK)
		printf("Error in Saving\n");
	else
		printf("File saved Successfully\n");
}

void parse_check(const char *expected, char *found)
{
	if (strcasecmp(expected, found))
	{
		printf("Expected '%s ' found '%s '\n", expected, found);
		printf("Parse error, abnormal abortion\n");
		exit(0);
	}
}

void parse_doubles(FILE* file, const char *check, double p[3])
{
	char str[100];
	fscanf(file, "%s", str);
	parse_check(check, str);
	fscanf(file, "%lf %lf %lf", &p[0], &p[1], &p[2]);
	printf("%s %lf %lf %lf\n", check, p[0], p[1], p[2]);
}

void parse_rad(FILE *file, double *r)
{
	char str[100];
	fscanf(file, "%s", str);
	parse_check("rad:", str);
	fscanf(file, "%lf", r);
	printf("rad: %f\n", *r);
}

void parse_shi(FILE *file, double *shi)
{
	char s[100];
	fscanf(file, "%s", s);
	parse_check("shi:", s);
	fscanf(file, "%lf", shi);
	printf("shi: %f\n", *shi);
}

int loadScene(char *argv)
{
	FILE * file = fopen(argv, "r");
	int number_of_objects;
	char type[50];
	Triangle t;
	Sphere s;
	Light l;
	fscanf(file, "%i", &number_of_objects);

	printf("number of objects: %i\n", number_of_objects);

	parse_doubles(file, "amb:", ambient_light);

	for (int i = 0; i < number_of_objects; i++)
	{
		fscanf(file, "%s\n", type);
		printf("%s\n", type);
		if (strcasecmp(type, "triangle") == 0)
		{
			printf("found triangle\n");
			for (int j = 0; j < 3; j++)
			{
				parse_doubles(file, "pos:", t.v[j].position);
				parse_doubles(file, "nor:", t.v[j].normal);
				parse_doubles(file, "dif:", t.v[j].color_diffuse);
				parse_doubles(file, "spe:", t.v[j].color_specular);
				parse_shi(file, &t.v[j].shininess);
			}

			if (num_triangles == MAX_TRIANGLES)
			{
				printf("too many triangles, you should increase MAX_TRIANGLES!\n");
				exit(0);
			}
			triangles[num_triangles++] = t;
		}
		else if (strcasecmp(type, "sphere") == 0)
		{
			printf("found sphere\n");

			parse_doubles(file, "pos:", s.position);
			parse_rad(file, &s.radius);
			parse_doubles(file, "dif:", s.color_diffuse);
			parse_doubles(file, "spe:", s.color_specular);
			parse_shi(file, &s.shininess);

			if (num_spheres == MAX_SPHERES)
			{
				printf("too many spheres, you should increase MAX_SPHERES!\n");
				exit(0);
			}
			spheres[num_spheres++] = s;
		}
		else if (strcasecmp(type, "light") == 0)
		{
			printf("found light\n");
			parse_doubles(file, "pos:", l.position);
			parse_doubles(file, "col:", l.color);

			if (num_lights == MAX_LIGHTS)
			{
				printf("too many lights, you should increase MAX_LIGHTS!\n");
				exit(0);
			}
			lights[num_lights++] = l;
		}
		else
		{
			printf("unknown type in scene description:\n%s\n", type);
			exit(0);
		}
	}
	return 0;
}

void display()
{
}

void init()
{
	glMatrixMode(GL_PROJECTION);
	glOrtho(0, WIDTH, 0, HEIGHT, 1, -1);
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();

	glClearColor(0, 0, 0, 0);
	glClear(GL_COLOR_BUFFER_BIT);
}

void idle()
{
	//hack to make it only draw once
	static int once = 0;
	if (!once)
	{
		draw_scene();
		if (mode == MODE_JPEG)
			save_jpg();
	}
	once = 1;
}

void keyboardFunc(unsigned char key, int x, int y)
{
	switch (key)
	{
	case 27:
		exit(0);
		break;
	}
}

int main(int argc, char ** argv)
{
	if ((argc < 2) || (argc > 3))
	{
		printf("Usage: %s <input scenefile> [output jpegname]\n", argv[0]);
		exit(0);
	}
	if (argc == 3)
	{
		mode = MODE_JPEG;
		filename = argv[2];
	}
	else if (argc == 2)
		mode = MODE_DISPLAY;

	glutInit(&argc, argv);
	loadScene(argv[1]);

	glutInitDisplayMode(GLUT_RGBA | GLUT_SINGLE);
	glutInitWindowPosition(0, 0);
	glutInitWindowSize(WIDTH, HEIGHT);
	int window = glutCreateWindow("Ray Tracer");
	glutDisplayFunc(display);
	glutIdleFunc(idle);
	glutKeyboardFunc(keyboardFunc);
	init();
	glutMainLoop();
}

