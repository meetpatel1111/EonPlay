# GitHub Workflows

This directory contains GitHub Actions workflows for the Cross-Platform Media Player project.

## Workflows Overview

### 1. CI/CD Pipeline (`ci.yml`)
**Triggers:** Push to main/develop, Pull Requests
**Purpose:** Main continuous integration and deployment pipeline

**Jobs:**
- **Code Quality:** Formatting checks, static analysis with cppcheck
- **Build Matrix:** Cross-platform builds (Linux, Windows, macOS)
- **Coverage:** Test coverage analysis with lcov and Codecov integration
- **Documentation:** Automatic documentation generation and deployment
- **Security:** CodeQL security scanning
- **Deploy:** Release deployment on tags
- **Benchmark:** Performance benchmarking on main branch

**Artifacts:** Build packages for all platforms, coverage reports

### 2. Nightly Builds (`nightly.yml`)
**Triggers:** Daily at 2 AM UTC, Manual dispatch
**Purpose:** Regular automated builds and extended testing

**Jobs:**
- **Nightly Build:** Full builds with extended test timeouts
- **Memory Leak Detection:** Valgrind memory leak analysis
- **Artifact Cleanup:** Automatic cleanup of old nightly builds

**Artifacts:** Nightly build packages (7-day retention)

### 3. Release (`release.yml`)
**Triggers:** Git tags matching `v*.*.*`
**Purpose:** Automated release creation and distribution

**Jobs:**
- **Create Release:** Generate changelog and create GitHub release
- **Build Release:** Platform-specific release builds with installers
- **Publish Packages:** Publish to package managers (npm, Homebrew, Chocolatey)

**Artifacts:** Release installers (.msi, .dmg, .deb, .rpm, .tar.gz)

### 4. Dependency Updates (`dependencies.yml`)
**Triggers:** Weekly on Mondays, Manual dispatch
**Purpose:** Automated dependency management and security auditing

**Jobs:**
- **Update Dependencies:** Check and update Qt, CMake, VLC versions
- **Security Audit:** Security scanning with clang-tidy
- **License Compliance:** License header and third-party license checks

**Artifacts:** License reports, security scan results

### 5. Docker (`docker.yml`)
**Triggers:** Push to main/develop, Tags, Pull Requests
**Purpose:** Container image building and deployment

**Jobs:**
- **Build and Push:** Multi-platform Docker image builds
- **Security Scan:** Container vulnerability scanning with Trivy
- **Deploy Staging:** Staging environment deployment
- **Deploy Production:** Production deployment on releases

**Artifacts:** Docker images in GitHub Container Registry

### 6. Documentation (`docs.yml`)
**Triggers:** Changes to docs, source code, or markdown files
**Purpose:** Documentation generation and deployment

**Jobs:**
- **Build Docs:** Doxygen API docs and Sphinx user guides
- **Deploy Docs:** GitHub Pages deployment
- **Check Links:** Markdown link validation

**Artifacts:** Generated documentation, link check reports

## Workflow Configuration

### Environment Variables
```yaml
QT_VERSION: '6.5.3'        # Qt framework version
CMAKE_VERSION: '3.27.7'    # CMake build system version
```

### Secrets Required
- `GITHUB_TOKEN`: Automatically provided by GitHub
- `NPM_TOKEN`: For npm package publishing (optional)
- `CODECOV_TOKEN`: For Codecov integration (optional)

### Branch Protection
Recommended branch protection rules for `main`:
- Require status checks to pass before merging
- Require branches to be up to date before merging
- Required status checks:
  - `Code Quality`
  - `Build (ubuntu-latest)`
  - `Build (windows-latest)`
  - `Build (macos-latest)`
  - `Test Coverage`

## Platform-Specific Notes

### Linux (Ubuntu)
- Uses system package manager for dependencies
- Supports hardware acceleration (VA-API, VDPAU)
- Generates .deb and .rpm packages

### Windows
- Downloads VLC SDK during build
- Uses Visual Studio 2022 for compilation
- Generates .msi installer with WiX
- Includes code signing support (if certificate provided)

### macOS
- Uses Homebrew for dependencies
- Generates .dmg disk image
- Supports universal binaries (Intel + Apple Silicon)

## Customization

### Adding New Platforms
1. Add platform to build matrix in `ci.yml`
2. Add platform-specific dependency installation
3. Configure appropriate CMake generator
4. Add packaging commands for the platform

### Modifying Dependencies
1. Update version numbers in workflow environment variables
2. Modify dependency installation commands
3. Update CMakeLists.txt if needed
4. Test on all platforms

### Adding Security Scans
1. Add new job to appropriate workflow
2. Configure scanning tool
3. Set up result upload to GitHub Security tab
4. Add failure conditions as needed

## Monitoring and Maintenance

### Regular Tasks
- Review dependency update PRs weekly
- Monitor nightly build failures
- Update Qt/CMake versions quarterly
- Review security scan results

### Troubleshooting
- Check workflow logs in GitHub Actions tab
- Verify secrets are properly configured
- Ensure branch protection rules are not blocking
- Check artifact storage limits

### Performance Optimization
- Use caching for dependencies (Qt, build artifacts)
- Parallelize independent jobs
- Use matrix builds for platform testing
- Optimize Docker layer caching

## Integration with External Services

### Codecov
Provides test coverage reporting and analysis.
Configuration in `.codecov.yml` (create if needed).

### GitHub Pages
Hosts documentation at `https://username.github.io/repository-name/`
Custom domain can be configured in repository settings.

### Container Registry
Images are published to `ghcr.io/username/repository-name`
Supports multi-platform images (amd64, arm64).

## Security Considerations

- All workflows use pinned action versions
- Secrets are properly scoped and not logged
- Container images are scanned for vulnerabilities
- Code is analyzed for security issues
- Dependencies are regularly updated

## Contributing

When modifying workflows:
1. Test changes in a fork first
2. Use semantic commit messages
3. Update this documentation
4. Consider backward compatibility
5. Test on all target platforms