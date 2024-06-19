#include "indexPageFilter.h"

void(CEF_CALLBACK add_ref)(struct indexPageFilter* self) {
    self->count += 1;
};

int(CEF_CALLBACK release)(struct indexPageFilter* self) {
    if (--(self->count) != 0) return false;
    delete self;
    return true;
};

int(CEF_CALLBACK has_one_ref)(struct indexPageFilter* self) {
    return self->count == 1;
}

int(CEF_CALLBACK has_at_least_one_ref)(struct indexPageFilter* self) {
    return self->count > 0;
};

int(CEF_CALLBACK init_filter)(struct indexPageFilter* self) {
    return true;
};

cef_response_filter_status_t(CEF_CALLBACK filter)(
    struct response_filter* self,
    void* data_in,
    size_t data_in_size,
    size_t* data_in_read,
    void* data_out,
    size_t data_out_size,
    size_t* data_out_written) {

    size_t pos = std::string::npos;
    {
        const char* data_in_ptr = static_cast<char*>(data_in);
        std::string src(data_in_ptr, data_in_size);
        const size_t sp = src.find("<script");
        const size_t hp = src.find("</head");

        if (sp != std::string::npos && hp != std::string::npos) {
            pos = sp > hp ? hp : sp;
        } else if (sp != std::string::npos) {
            pos = sp;
        } else if (hp != std::string::npos) {
            pos = hp;
        }
    }

    if (pos != std::string::npos) {
        memcpy(data_out, data_in, pos);
        std::string fragment = "<script>debugger;</script>";
        memcpy(static_cast<char*>(data_out) + pos, fragment.c_str(), fragment.length());
        memcpy(static_cast<char*>(data_out) + pos + fragment.length(), static_cast<char*>(data_in) + pos, data_in_size - pos);
        *data_out_written = fragment.length() + data_in_size;

        *data_in_read = data_in_size;
    } else {
        *data_out_written = data_in_size < data_out_size ? data_in_size : data_out_size;
        if (*data_out_written > 0) {
            memcpy(data_out, data_in, *data_out_written);
            *data_in_read = *data_out_written;
        }
    }

    return RESPONSE_FILTER_DONE;
};

indexPageFilter::indexPageFilter() {
    this->base.base.size = sizeof(indexPageFilter);
    this->base.base.add_ref = decltype(this->base.base.add_ref)(&add_ref);
    this->base.base.release = decltype(this->base.base.release)(&release);
    this->base.base.has_one_ref = decltype(this->base.base.has_one_ref)(&has_one_ref);
    this->base.base.has_at_least_one_ref = decltype(this->base.base.has_at_least_one_ref)(&has_at_least_one_ref);

    this->base.init_filter = decltype(this->base.init_filter)(&init_filter);
    this->base.filter = decltype(this->base.filter)(&filter);

    this->count = 0;
}