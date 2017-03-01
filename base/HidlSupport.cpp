/*
 * Copyright (C) 2016 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#define LOG_TAG "HidlSupport"

#include <hidl/HidlSupport.h>

#include <unordered_map>

#include <android-base/logging.h>
#include <android-base/parseint.h>
#include <hidl-util/FQName.h>
#include <vintf/VintfObject.h>
#include <vintf/parse_string.h>

namespace android {
namespace hardware {

vintf::Transport getTransportFromManifest(
        const FQName &fqName, const std::string &manifestName,
        const vintf::HalManifest *vm) {
    if (vm == nullptr) {
        LOG(WARNING) << "getTransportFromManifest: No " << manifestName << " manifest defined, "
                     << "using default transport for " << fqName.string();
        return vintf::Transport::EMPTY;
    }
    vintf::Transport tr = vm->getTransport(fqName.package(),
            vintf::Version{fqName.getPackageMajorVersion(), fqName.getPackageMinorVersion()});
    if (tr == vintf::Transport::EMPTY) {
        LOG(WARNING) << "getTransportFromManifest: Cannot find entry "
                     << fqName.string()
                     << " in " << manifestName << " manifest, using default transport.";
    } else {
        LOG(DEBUG) << "getTransportFromManifest: " << fqName.string()
                   << " declares transport method " << to_string(tr)
                   << " in " << manifestName << " manifest";
    }
    return tr;
}

vintf::Transport getTransport(const std::string &name) {
    FQName fqName(name);
    if (!fqName.isValid()) {
        LOG(ERROR) << "getTransport: " << name << " is not a valid fully-qualified name.";
        return vintf::Transport::EMPTY;
    }
    if (!fqName.hasVersion()) {
        LOG(ERROR) << "getTransport: " << fqName.string()
                   << " does not specify a version. Using default transport.";
        return vintf::Transport::EMPTY;
    }
    // TODO(b/34772739): modify the list if other packages are added to system/manifest.xml
    if (fqName.inPackage("android.hidl")) {
        return getTransportFromManifest(fqName, "framework",
                vintf::VintfObject::GetFrameworkHalManifest());
    }
    return getTransportFromManifest(fqName, "device",
            vintf::VintfObject::GetDeviceHalManifest());
}

hidl_handle::hidl_handle() {
    mHandle = nullptr;
    mOwnsHandle = false;
}

hidl_handle::~hidl_handle() {
    freeHandle();
}

hidl_handle::hidl_handle(const native_handle_t *handle) {
    mHandle = handle;
    mOwnsHandle = false;
}

// copy constructor.
hidl_handle::hidl_handle(const hidl_handle &other) {
    mOwnsHandle = false;
    *this = other;
}

// move constructor.
hidl_handle::hidl_handle(hidl_handle &&other) {
    mOwnsHandle = false;
    *this = std::move(other);
}

// assignment operators
hidl_handle &hidl_handle::operator=(const hidl_handle &other) {
    if (this == &other) {
        return *this;
    }
    freeHandle();
    if (other.mHandle != nullptr) {
        mHandle = native_handle_clone(other.mHandle);
        if (mHandle == nullptr) {
            LOG(FATAL) << "Failed to clone native_handle in hidl_handle.";
        }
        mOwnsHandle = true;
    } else {
        mHandle = nullptr;
        mOwnsHandle = false;
    }
    return *this;
}

hidl_handle &hidl_handle::operator=(const native_handle_t *native_handle) {
    freeHandle();
    mHandle = native_handle;
    mOwnsHandle = false;
    return *this;
}

hidl_handle &hidl_handle::operator=(hidl_handle &&other) {
    if (this != &other) {
        freeHandle();
        mHandle = other.mHandle;
        mOwnsHandle = other.mOwnsHandle;
        other.mHandle = nullptr;
        other.mOwnsHandle = false;
    }
    return *this;
}

void hidl_handle::setTo(native_handle_t* handle, bool shouldOwn) {
    freeHandle();
    mHandle = handle;
    mOwnsHandle = shouldOwn;
}

const native_handle_t* hidl_handle::operator->() const {
    return mHandle;
}

// implicit conversion to const native_handle_t*
hidl_handle::operator const native_handle_t *() const {
    return mHandle;
}

// explicit conversion
const native_handle_t *hidl_handle::getNativeHandle() const {
    return mHandle;
}

void hidl_handle::freeHandle() {
    if (mOwnsHandle && mHandle != nullptr) {
        // This can only be true if:
        // 1. Somebody called setTo() with shouldOwn=true, so we know the handle
        //    wasn't const to begin with.
        // 2. Copy/assignment from another hidl_handle, in which case we have
        //    cloned the handle.
        // 3. Move constructor from another hidl_handle, in which case the original
        //    hidl_handle must have been non-const as well.
        native_handle_t *handle = const_cast<native_handle_t*>(
                static_cast<const native_handle_t*>(mHandle));
        native_handle_close(handle);
        native_handle_delete(handle);
        mHandle = nullptr;
    }
}

static const char *const kEmptyString = "";

hidl_string::hidl_string()
    : mBuffer(kEmptyString),
      mSize(0),
      mOwnsBuffer(false) {
}

hidl_string::~hidl_string() {
    clear();
}

hidl_string::hidl_string(const char *s) : hidl_string() {
    if (s == nullptr) {
        return;
    }

    copyFrom(s, strlen(s));
}

hidl_string::hidl_string(const char *s, size_t length) : hidl_string() {
    copyFrom(s, length);
}

hidl_string::hidl_string(const hidl_string &other): hidl_string() {
    copyFrom(other.c_str(), other.size());
}

hidl_string::hidl_string(const std::string &s) : hidl_string() {
    copyFrom(s.c_str(), s.size());
}

hidl_string::hidl_string(hidl_string &&other): hidl_string() {
    moveFrom(std::forward<hidl_string>(other));
}

hidl_string &hidl_string::operator=(hidl_string &&other) {
    if (this != &other) {
        clear();
        moveFrom(std::forward<hidl_string>(other));
    }
    return *this;
}

hidl_string &hidl_string::operator=(const hidl_string &other) {
    if (this != &other) {
        clear();
        copyFrom(other.c_str(), other.size());
    }

    return *this;
}

hidl_string &hidl_string::operator=(const char *s) {
    clear();

    if (s == nullptr) {
        return *this;
    }

    copyFrom(s, strlen(s));
    return *this;
}

hidl_string &hidl_string::operator=(const std::string &s) {
    clear();
    copyFrom(s.c_str(), s.size());
    return *this;
}

hidl_string::operator std::string() const {
    return std::string(mBuffer, mSize);
}

hidl_string::operator const char *() const {
    return mBuffer;
}

void hidl_string::copyFrom(const char *data, size_t size) {
    // assume my resources are freed.

    if (size > UINT32_MAX) {
        LOG(FATAL) << "string size can't exceed 2^32 bytes.";
    }
    char *buf = (char *)malloc(size + 1);
    memcpy(buf, data, size);
    buf[size] = '\0';
    mBuffer = buf;

    mSize = static_cast<uint32_t>(size);
    mOwnsBuffer = true;
}

void hidl_string::moveFrom(hidl_string &&other) {
    // assume my resources are freed.

    mBuffer = other.mBuffer;
    mSize = other.mSize;
    mOwnsBuffer = other.mOwnsBuffer;

    other.mOwnsBuffer = false;
}

void hidl_string::clear() {
    if (mOwnsBuffer && (mBuffer != kEmptyString)) {
        free(const_cast<char *>(static_cast<const char *>(mBuffer)));
    }

    mBuffer = kEmptyString;
    mSize = 0;
    mOwnsBuffer = false;
}

void hidl_string::setToExternal(const char *data, size_t size) {
    if (size > UINT32_MAX) {
        LOG(FATAL) << "string size can't exceed 2^32 bytes.";
    }
    clear();

    mBuffer = data;
    mSize = static_cast<uint32_t>(size);
    mOwnsBuffer = false;
}

const char *hidl_string::c_str() const {
    return mBuffer;
}

size_t hidl_string::size() const {
    return mSize;
}

bool hidl_string::empty() const {
    return mSize == 0;
}

}  // namespace hardware
}  // namespace android


