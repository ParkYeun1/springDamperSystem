#ifndef CLOTH_H
#define CLOTH_H

// Cloth: a grid of point masses connected by spring-dampers (f = -kx - dv),
// integrated with semi-implicit Euler and stabilised with Provot stretch-limiting.

#include <glad/glad.h>
#include <glm/glm.hpp>
#include <vector>
#include <cmath>

struct Particle {
	glm::vec3 pos    = glm::vec3(0.0f);
	glm::vec3 vel    = glm::vec3(0.0f);
	glm::vec3 force  = glm::vec3(0.0f);
	glm::vec3 normal = glm::vec3(0.0f, 0.0f, 1.0f);
	bool      pinned = false;
};

struct Spring {
	int   a = 0, b = 0;
	float restLength = 0.0f;
};

class Cloth {
public:
	int   N;        // particles per side (N x N)
	float size;     // world-space size of the sheet

	float k          = 80.0f;
	float d          = 1.0f;
	float mass       = 1.0f;
	float gravity    = 2.0f;
	float airDamp    = 0.05f;
	float globalDamp = 0.99f;
	float dt         = 1.0f / 240.0f;
	int   substeps   = 8;
	float maxStretch = 0.1f;         // Provot limit: max 10% stretch
	int   constraintIters = 2;

	bool      windEnabled  = true;
	glm::vec3 windDir       = glm::vec3(0.3f, 0.05f, 1.0f);
	float     windStrength  = 3.0f;

	int pinMode = 0; // 0 = top corners, 1 = top row, 2 = none

	bool      colliderEnabled = false;
	glm::vec3 colliderCenter  = glm::vec3(0.0f, -1.0f, 0.5f);
	float     colliderRadius  = 1.2f;

	Cloth(int n = 24, float s = 6.0f) : N(n), size(s) {
		buildGrid();
		buildSprings();
		buildIndices();
		setupGL();
	}
	~Cloth() {
		if (EBO) glDeleteBuffers(1, &EBO);
		if (VBO) glDeleteBuffers(1, &VBO);
		if (VAO) glDeleteVertexArrays(1, &VAO);
	}

	void reset() {
		buildGrid();
		buildSprings();
	}

	void simulate() {
		for (int s = 0; s < substeps; ++s)
			step();
	}

	void draw() {
		computeNormals();
		uploadMesh();
		glBindVertexArray(VAO);
		glDrawElements(GL_TRIANGLES, (GLsizei)indices.size(), GL_UNSIGNED_INT, 0);
		glBindVertexArray(0);
	}

private:
	std::vector<Particle>     particles;
	std::vector<Spring>       springs;
	std::vector<unsigned int> indices;
	std::vector<float>        vertexData; // interleaved pos(3) + normal(3)
	unsigned int VAO = 0, VBO = 0, EBO = 0;
	float simTime = 0.0f;

	int idx(int i, int j) const { return j * N + i; }

	void buildGrid() {
		particles.assign(N * N, Particle());
		float spacing = size / (float)(N - 1);
		for (int j = 0; j < N; ++j)
			for (int i = 0; i < N; ++i) {
				Particle& p = particles[idx(i, j)];
				p.pos    = glm::vec3(i * spacing - size * 0.5f,
				                     j * spacing - size * 0.5f, 0.0f);
				p.vel    = glm::vec3(0.0f);
				p.force  = glm::vec3(0.0f);
				p.normal = glm::vec3(0.0f, 0.0f, 1.0f);
			}
		applyPins();
	}

	void applyPins() {
		for (auto& p : particles) p.pinned = false;
		if (pinMode == 0) {
			particles[idx(0,     N - 1)].pinned = true;
			particles[idx(N - 1, N - 1)].pinned = true;
		} else if (pinMode == 1) {
			for (int i = 0; i < N; ++i)
				particles[idx(i, N - 1)].pinned = true;
		}
	}

	void buildSprings() {
		springs.clear();
		auto add = [&](int a, int b) {
			Spring s;
			s.a = a; s.b = b;
			s.restLength = glm::length(particles[a].pos - particles[b].pos);
			springs.push_back(s);
		};
		for (int j = 0; j < N; ++j)
			for (int i = 0; i < N; ++i) {
				// structural
				if (i + 1 < N) add(idx(i, j), idx(i + 1, j));
				if (j + 1 < N) add(idx(i, j), idx(i, j + 1));
				// shear
				if (i + 1 < N && j + 1 < N) {
					add(idx(i, j),     idx(i + 1, j + 1));
					add(idx(i + 1, j), idx(i,     j + 1));
				}
				// bend
				if (i + 2 < N) add(idx(i, j), idx(i + 2, j));
				if (j + 2 < N) add(idx(i, j), idx(i, j + 2));
			}
	}

	void buildIndices() {
		indices.clear();
		for (int j = 0; j < N - 1; ++j)
			for (int i = 0; i < N - 1; ++i) {
				int i0 = idx(i,     j);
				int i1 = idx(i + 1, j);
				int i2 = idx(i,     j + 1);
				int i3 = idx(i + 1, j + 1);
				indices.push_back(i0); indices.push_back(i2); indices.push_back(i1);
				indices.push_back(i1); indices.push_back(i2); indices.push_back(i3);
			}
	}

	void step() {
		simTime += dt;
		accumulateForces();
		integrate();
		applyConstraints();
	}

	void accumulateForces() {
		glm::vec3 wdir = glm::normalize(windDir);
		for (auto& p : particles) {
			if (p.pinned) { p.force = glm::vec3(0.0f); continue; }
			p.force  = glm::vec3(0.0f, -mass * gravity, 0.0f);
			p.force -= airDamp * p.vel;
			if (windEnabled) {
				float gust = 0.55f + 0.45f * std::sin(simTime * 1.7f
				                                      + p.pos.x * 0.7f
				                                      + p.pos.y * 0.4f);
				glm::vec3 w = wdir * (windStrength * gust);
				// aerodynamic: push along the face normal so the cloth billows
				glm::vec3 rel = w - p.vel;
				p.force += glm::dot(p.normal, rel) * p.normal;
			}
		}
		for (const auto& s : springs) {
			Particle& A = particles[s.a];
			Particle& B = particles[s.b];
			glm::vec3 delta = B.pos - A.pos;
			float len = glm::length(delta);
			if (len < 1e-6f) continue;
			glm::vec3 dir = delta / len;
			float fk = k * (len - s.restLength); // -kx
			float fd = d * glm::dot(B.vel - A.vel, dir); // -dv
			glm::vec3 f = (fk + fd) * dir;
			if (!A.pinned) A.force += f;
			if (!B.pinned) B.force -= f;
		}
	}

	void integrate() {
		for (auto& p : particles) {
			if (p.pinned) { p.vel = glm::vec3(0.0f); continue; }
			glm::vec3 acc = p.force / mass;
			p.vel += acc * dt;
			p.vel *= globalDamp;
			p.pos += p.vel * dt;
		}
	}

	void applyConstraints() {
		for (int it = 0; it < constraintIters; ++it) {
			// Provot stretch limiting
			for (const auto& s : springs) {
				Particle& A = particles[s.a];
				Particle& B = particles[s.b];
				glm::vec3 delta = B.pos - A.pos;
				float len = glm::length(delta);
				if (len < 1e-6f) continue;
				float maxLen = s.restLength * (1.0f + maxStretch);
				if (len <= maxLen) continue;
				glm::vec3 dir = delta / len;
				float over = len - maxLen;
				if (A.pinned && B.pinned)      continue;
				else if (A.pinned)             B.pos -= dir * over;
				else if (B.pinned)             A.pos += dir * over;
				else { A.pos += dir * (over * 0.5f); B.pos -= dir * (over * 0.5f); }
			}
			if (colliderEnabled) {
				float r = colliderRadius + 0.02f;
				for (auto& p : particles) {
					if (p.pinned) continue;
					glm::vec3 dc = p.pos - colliderCenter;
					float dist = glm::length(dc);
					if (dist < r && dist > 1e-6f) {
						glm::vec3 n = dc / dist;
						p.pos = colliderCenter + n * r;
						float vn = glm::dot(p.vel, n);
						if (vn < 0.0f) p.vel -= vn * n;
					}
				}
			}
		}
	}

	void computeNormals() {
		for (auto& p : particles) p.normal = glm::vec3(0.0f);
		for (size_t t = 0; t + 2 < indices.size() + 1; t += 3) {
			unsigned int a = indices[t], b = indices[t + 1], c = indices[t + 2];
			glm::vec3 fn = glm::cross(particles[b].pos - particles[a].pos,
			                          particles[c].pos - particles[a].pos);
			particles[a].normal += fn;
			particles[b].normal += fn;
			particles[c].normal += fn;
		}
		for (auto& p : particles) {
			if (glm::length(p.normal) > 1e-6f) p.normal = glm::normalize(p.normal);
			else p.normal = glm::vec3(0.0f, 0.0f, 1.0f);
		}
	}

	void setupGL() {
		glGenVertexArrays(1, &VAO);
		glGenBuffers(1, &VBO);
		glGenBuffers(1, &EBO);
		glBindVertexArray(VAO);
		glBindBuffer(GL_ARRAY_BUFFER, VBO);
		glBufferData(GL_ARRAY_BUFFER, particles.size() * 6 * sizeof(float),
		             nullptr, GL_DYNAMIC_DRAW);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int),
		             indices.data(), GL_STATIC_DRAW);
		glEnableVertexAttribArray(0);
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
		glEnableVertexAttribArray(1);
		glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float),
		                      (void*)(3 * sizeof(float)));
		glBindVertexArray(0);
	}

	void uploadMesh() {
		vertexData.resize(particles.size() * 6);
		for (size_t i = 0; i < particles.size(); ++i) {
			vertexData[i * 6 + 0] = particles[i].pos.x;
			vertexData[i * 6 + 1] = particles[i].pos.y;
			vertexData[i * 6 + 2] = particles[i].pos.z;
			vertexData[i * 6 + 3] = particles[i].normal.x;
			vertexData[i * 6 + 4] = particles[i].normal.y;
			vertexData[i * 6 + 5] = particles[i].normal.z;
		}
		glBindVertexArray(VAO);
		glBindBuffer(GL_ARRAY_BUFFER, VBO);
		glBufferSubData(GL_ARRAY_BUFFER, 0,
		                vertexData.size() * sizeof(float), vertexData.data());
		glBindVertexArray(0);
	}
};

#endif // CLOTH_H
