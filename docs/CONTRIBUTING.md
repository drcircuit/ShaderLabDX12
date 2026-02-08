# Contributing to ShaderLab

Thank you for your interest in contributing to ShaderLab! This document provides guidelines for contributing to the project.

## How to Contribute

### Reporting Bugs

1. Check existing issues to avoid duplicates
2. Open a new issue with:
   - Clear description of the problem
   - Steps to reproduce
   - Expected vs actual behavior
   - System information (OS, GPU, driver version)
   - Relevant logs or screenshots

### Suggesting Features

1. Open an issue with the "enhancement" tag
2. Describe the feature and its use case
3. Explain how it fits ShaderLab's philosophy (minimalism, demoscene focus)
4. Be open to discussion and alternative approaches

### Submitting Code

1. Fork the repository
2. Create a feature branch (`git checkout -b feature/amazing-feature`)
3. Make your changes following our code style
4. Test thoroughly in Debug and Release builds
5. Commit with clear messages
6. Push to your fork
7. Open a Pull Request

### Code Style Guidelines

- **C++20** standard features are encouraged
- **RAII** for resource management
- Use **smart pointers** for ownership, raw pointers for non-owning references
- Clear, descriptive names (no Hungarian notation)
- Comments only where intent isn't obvious from code
- Consistent formatting (follow existing code style)

Example:
```cpp
class MySystem {
public:
    bool Initialize();  // Clear verb-based names
    void Shutdown();
    
private:
    std::unique_ptr<Resource> m_resource;  // Member prefix
    int m_counter = 0;
};
```

### Commit Messages

- Use present tense ("Add feature" not "Added feature")
- Use imperative mood ("Move cursor to..." not "Moves cursor to...")
- Limit first line to 72 characters
- Reference issues and PRs when relevant

Example:
```
Add beat-synchronized particle system

- Implement particle emitter with beat phase sync
- Add particle rendering to Scene View
- Update shader constants to include particle data

Closes #42
```

### Testing

- Test on your hardware before submitting
- Verify in both Debug and Release configurations
- Check for memory leaks (use debug tools if available)
- Ensure backward compatibility with existing projects

### Documentation

- Update README.md if adding major features
- Add comments for complex algorithms
- Update ARCHITECTURE.md for structural changes
- Include example shaders if adding rendering features

## Contribution Areas

We especially welcome contributions in:

### Core Features
- Multi-pass rendering system
- Shader parameter automation
- Playlist and transitions
- Texture loading and management
- Export to standalone runtime

### Creative Content
- Example shaders (licensed under CC BY-NC-SA 4.0)
- Demo projects
- Tutorials and documentation
- Shader presets and templates

### Tools & Utilities
- Build scripts
- Shader validator tools
- Asset converters
- Profiling tools

### Platform Support
- AMD GPU optimization
- Intel GPU support
- Laptop/low-power testing

## License Agreement

By contributing code to ShaderLab, you agree to license your contributions under the Community License (see LICENSE-COMMUNITY.md).

By contributing creative assets (shaders, examples), you agree to license them under CC BY-NC-SA 4.0.

## Community Guidelines

- Be respectful and constructive
- Focus on what's best for the project and community
- Embrace the demoscene spirit: creativity, learning, and sharing
- Help newcomers and answer questions
- Keep discussions on-topic

## Getting Help

- Open an issue for bugs or feature requests
- Use GitHub Discussions for questions and ideas
- Check existing documentation first

## Recognition

Contributors will be:
- Listed in project credits
- Mentioned in release notes for their contributions
- Part of the growing ShaderLab community

Thank you for helping make ShaderLab better!

---

*Questions? Open an issue or start a discussion on GitHub.*
