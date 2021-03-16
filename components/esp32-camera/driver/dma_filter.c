
static void dma_filter_jpeg(const dma_elem_t* src, lldesc_t* dma_desc, uint8_t* dst)
{
    size_t end = dma_desc->length / sizeof(dma_elem_t) / 4;
    // manually unrolling 4 iterations of the loop here
    for (size_t i = 0; i < end; ++i) {
        dst[0] = src[0].sample1;
        dst[1] = src[1].sample1;
        dst[2] = src[2].sample1;
        dst[3] = src[3].sample1;
        src += 4;
        dst += 4;
    }
}

static void dma_filter_grayscale(const dma_elem_t* src, lldesc_t* dma_desc, uint8_t* dst)
{
    size_t end = dma_desc->length / sizeof(dma_elem_t) / 4;
    for (size_t i = 0; i < end; ++i) {
        // manually unrolling 4 iterations of the loop here
        dst[0] = src[0].sample1;
        dst[1] = src[1].sample1;
        dst[2] = src[2].sample1;
        dst[3] = src[3].sample1;
        src += 4;
        dst += 4;
    }
}

static void dma_filter_grayscale_highspeed(const dma_elem_t* src, lldesc_t* dma_desc, uint8_t* dst)
{
    size_t end = dma_desc->length / sizeof(dma_elem_t) / 8;
    for (size_t i = 0; i < end; ++i) {
        // manually unrolling 4 iterations of the loop here
        dst[0] = src[0].sample1;
        dst[1] = src[2].sample1;
        dst[2] = src[4].sample1;
        dst[3] = src[6].sample1;
        src += 8;
        dst += 4;
    }
    // the final sample of a line in SM_0A0B_0B0C sampling mode needs special handling
    if ((dma_desc->length & 0x7) != 0) {
        dst[0] = src[0].sample1;
        dst[1] = src[2].sample1;
    }
}

static void dma_filter_yuyv(const dma_elem_t* src, lldesc_t* dma_desc, uint8_t* dst)
{
    size_t end = dma_desc->length / sizeof(dma_elem_t) / 4;
    for (size_t i = 0; i < end; ++i) {
        dst[0] = src[0].sample1;//y0
        dst[1] = src[0].sample2;//u
        dst[2] = src[1].sample1;//y1
        dst[3] = src[1].sample2;//v

        dst[4] = src[2].sample1;//y0
        dst[5] = src[2].sample2;//u
        dst[6] = src[3].sample1;//y1
        dst[7] = src[3].sample2;//v
        src += 4;
        dst += 8;
    }
}

static void dma_filter_yuyv_highspeed(const dma_elem_t* src, lldesc_t* dma_desc, uint8_t* dst)
{
    size_t end = dma_desc->length / sizeof(dma_elem_t) / 8;
    for (size_t i = 0; i < end; ++i) {
        dst[0] = src[0].sample1;//y0
        dst[1] = src[1].sample1;//u
        dst[2] = src[2].sample1;//y1
        dst[3] = src[3].sample1;//v

        dst[4] = src[4].sample1;//y0
        dst[5] = src[5].sample1;//u
        dst[6] = src[6].sample1;//y1
        dst[7] = src[7].sample1;//v
        src += 8;
        dst += 8;
    }
    if ((dma_desc->length & 0x7) != 0) {
        dst[0] = src[0].sample1;//y0
        dst[1] = src[1].sample1;//u
        dst[2] = src[2].sample1;//y1
        dst[3] = src[2].sample2;//v
    }
}

static void dma_filter_rgb888(const dma_elem_t* src, lldesc_t* dma_desc, uint8_t* dst)
{
    size_t end = dma_desc->length / sizeof(dma_elem_t) / 4;
    uint8_t lb, hb;
    for (size_t i = 0; i < end; ++i) {
        hb = src[0].sample1;
        lb = src[0].sample2;
        dst[0] = (lb & 0x1F) << 3;
        dst[1] = (hb & 0x07) << 5 | (lb & 0xE0) >> 3;
        dst[2] = hb & 0xF8;

        hb = src[1].sample1;
        lb = src[1].sample2;
        dst[3] = (lb & 0x1F) << 3;
        dst[4] = (hb & 0x07) << 5 | (lb & 0xE0) >> 3;
        dst[5] = hb & 0xF8;

        hb = src[2].sample1;
        lb = src[2].sample2;
        dst[6] = (lb & 0x1F) << 3;
        dst[7] = (hb & 0x07) << 5 | (lb & 0xE0) >> 3;
        dst[8] = hb & 0xF8;

        hb = src[3].sample1;
        lb = src[3].sample2;
        dst[9] = (lb & 0x1F) << 3;
        dst[10] = (hb & 0x07) << 5 | (lb & 0xE0) >> 3;
        dst[11] = hb & 0xF8;
        src += 4;
        dst += 12;
    }
}

static void IRAM_ATTR dma_filter_rgb888_highspeed(const dma_elem_t* src, lldesc_t* dma_desc, uint8_t* dst)
{
    size_t end = dma_desc->length / sizeof(dma_elem_t) / 8;
    uint8_t lb, hb;
    for (size_t i = 0; i < end; ++i) {
        hb = src[0].sample1;
        lb = src[1].sample1;
        dst[0] = (lb & 0x1F) << 3;
        dst[1] = (hb & 0x07) << 5 | (lb & 0xE0) >> 3;
        dst[2] = hb & 0xF8;

        hb = src[2].sample1;
        lb = src[3].sample1;
        dst[3] = (lb & 0x1F) << 3;
        dst[4] = (hb & 0x07) << 5 | (lb & 0xE0) >> 3;
        dst[5] = hb & 0xF8;

        hb = src[4].sample1;
        lb = src[5].sample1;
        dst[6] = (lb & 0x1F) << 3;
        dst[7] = (hb & 0x07) << 5 | (lb & 0xE0) >> 3;
        dst[8] = hb & 0xF8;

        hb = src[6].sample1;
        lb = src[7].sample1;
        dst[9] = (lb & 0x1F) << 3;
        dst[10] = (hb & 0x07) << 5 | (lb & 0xE0) >> 3;
        dst[11] = hb & 0xF8;

        src += 8;
        dst += 12;
    }
    if ((dma_desc->length & 0x7) != 0) {
        hb = src[0].sample1;
        lb = src[1].sample1;
        dst[0] = (lb & 0x1F) << 3;
        dst[1] = (hb & 0x07) << 5 | (lb & 0xE0) >> 3;
        dst[2] = hb & 0xF8;

        hb = src[2].sample1;
        lb = src[2].sample2;
        dst[3] = (lb & 0x1F) << 3;
        dst[4] = (hb & 0x07) << 5 | (lb & 0xE0) >> 3;
        dst[5] = hb & 0xF8;
    }
}
