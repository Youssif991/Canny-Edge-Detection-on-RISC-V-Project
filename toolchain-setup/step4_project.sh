#!/usr/bin/env bash
# -----------------------------------------------------------------------------
# step4_project.sh
# Create the project folder structure and build GoogleTest from source.
# -----------------------------------------------------------------------------

step4_project_structure() {
    info "Step 4/5 — Creating project structure and building GoogleTest..."

    # Project folders
    mkdir -p "$HOME/$PROJECT_TITLE"/{assets,include,src,tests,tools}
    mkdir -p "$HOME/$PROJECT_TITLE"/build/{host,target}

    success "Project structure created at $HOME/$PROJECT_TITLE"

    # GoogleTest
    if [ -d "$GTEST_INSTALL/lib" ]; then
        warn "GoogleTest already installed at $GTEST_INSTALL — skipping."
        return
    fi

    GTEST_SRC="$HOME/googletest"

    if [ ! -d "$GTEST_SRC/.git" ]; then
        git clone --depth 1 https://github.com/google/googletest.git "$GTEST_SRC"
    else
        info "GoogleTest source already cloned — skipping."
    fi

    cd "$GTEST_SRC"
    mkdir -p build && cd build

    cmake .. -DCMAKE_INSTALL_PREFIX="$GTEST_INSTALL"
    make -j"$JOBS"
    make install

    success "GoogleTest installed at $GTEST_INSTALL"
}
