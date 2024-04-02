
const byte img_data [] PROGMEM = {
	// 'esp32ressosmall, 64x128px
	0xff, 0xff, 0xff, 0x03, 0x00, 0x00, 0x00, 0x00, 0xff, 0xff, 0xff, 0x01, 0x10, 0x00, 0x00, 0x00, 
	0xff, 0xff, 0xff, 0x00, 0x18, 0x00, 0x00, 0x00, 0xff, 0xff, 0xff, 0x00, 0x80, 0x00, 0x00, 0x00, 
	0xff, 0xff, 0xff, 0x00, 0x80, 0x01, 0x00, 0x00, 0xff, 0xff, 0x7f, 0x00, 0x80, 0x01, 0x00, 0x00, 
	0xff, 0xff, 0x7f, 0x00, 0x80, 0x03, 0x00, 0x00, 0xff, 0xff, 0x7f, 0x00, 0x80, 0x07, 0x00, 0x00, 
	0xff, 0xff, 0x7f, 0x00, 0xe0, 0x1f, 0x00, 0x00, 0xff, 0xff, 0x7f, 0x00, 0xf8, 0x7f, 0x00, 0x00, 
	0xff, 0xff, 0x3f, 0x00, 0xfe, 0xff, 0x01, 0x00, 0xff, 0xff, 0x0f, 0x00, 0xff, 0xff, 0x03, 0x00, 
	0xff, 0x1f, 0x0c, 0x80, 0xff, 0xff, 0x07, 0x00, 0xff, 0x0f, 0x00, 0xc0, 0xff, 0xff, 0x0f, 0x00, 
	0xff, 0x07, 0x00, 0xc0, 0xf9, 0xff, 0x1f, 0x00, 0xff, 0x03, 0x00, 0xe0, 0xe1, 0xff, 0x1f, 0x00, 
	0xff, 0x01, 0x00, 0xe0, 0x80, 0xff, 0x3f, 0x00, 0xff, 0x01, 0x00, 0x70, 0x00, 0xff, 0x3f, 0x00, 
	0xff, 0x01, 0x00, 0x30, 0x00, 0xfe, 0x7f, 0x00, 0xff, 0x01, 0x00, 0x38, 0x00, 0xfc, 0x7f, 0x00, 
	0xff, 0x00, 0x00, 0x18, 0x00, 0xfc, 0xff, 0x00, 0xff, 0x00, 0x00, 0x1c, 0x00, 0xf8, 0xff, 0x00, 
	0xff, 0x00, 0x00, 0x1c, 0x00, 0xf0, 0xff, 0x00, 0xff, 0x01, 0x00, 0x0e, 0x00, 0xf0, 0xff, 0x01, 
	0xff, 0x01, 0x00, 0x0e, 0x00, 0xf0, 0xff, 0x01, 0xff, 0x40, 0x00, 0x0e, 0x00, 0xe0, 0xff, 0x01, 
	0xff, 0x60, 0x00, 0x0f, 0x80, 0xe0, 0xff, 0x01, 0xff, 0xfc, 0xbf, 0x0f, 0x00, 0xc0, 0xff, 0x01, 
	0xff, 0xfc, 0xff, 0x0f, 0x00, 0xc1, 0xff, 0x01, 0xff, 0xfc, 0xff, 0x07, 0xc0, 0xc5, 0xff, 0x01, 
	0xff, 0xfc, 0xff, 0x07, 0xe0, 0xc3, 0xff, 0x03, 0xff, 0xfc, 0xff, 0x07, 0xe0, 0x81, 0xff, 0x03, 
	0xff, 0xfe, 0xff, 0x07, 0xf0, 0x09, 0xfe, 0x03, 0xff, 0xfc, 0xff, 0x07, 0xf0, 0x8a, 0xfc, 0x03, 
	0xff, 0xfc, 0xff, 0x07, 0x70, 0xb6, 0xff, 0x01, 0xff, 0xfc, 0xff, 0x0f, 0x70, 0x80, 0xcf, 0x01, 
	0xff, 0xfd, 0xff, 0x0f, 0xe0, 0x10, 0x8f, 0x01, 0xff, 0xfd, 0xff, 0x0f, 0x08, 0x80, 0x0f, 0x03, 
	0xff, 0xf9, 0xff, 0x0f, 0x04, 0x80, 0x07, 0x02, 0xff, 0xfb, 0xff, 0x0f, 0x0c, 0x00, 0xe3, 0x04, 
	0xff, 0xff, 0xff, 0x1f, 0xb8, 0x00, 0xe0, 0x03, 0xff, 0xff, 0xff, 0x1f, 0xc0, 0x41, 0xe0, 0x0f, 
	0xff, 0xff, 0xff, 0x1f, 0x8c, 0xe1, 0xe0, 0x0f, 0xff, 0xff, 0xff, 0x3f, 0xb8, 0xea, 0xf3, 0x0f, 
	0xff, 0xff, 0xff, 0x3f, 0x70, 0xe8, 0xf3, 0x0f, 0xff, 0xff, 0xf1, 0x7f, 0xc0, 0x21, 0xf1, 0x0f, 
	0xff, 0xff, 0x83, 0x7f, 0x00, 0x87, 0xf1, 0x0f, 0xff, 0xff, 0x07, 0xff, 0x00, 0xac, 0xe1, 0x0f, 
	0xff, 0xff, 0x0f, 0xfe, 0x01, 0xb0, 0x80, 0x07, 0xff, 0xff, 0xdf, 0xfe, 0x03, 0xe0, 0x01, 0x06, 
	0xff, 0xff, 0xff, 0xfd, 0x07, 0x80, 0x07, 0x00, 0xff, 0xff, 0xff, 0xf9, 0x1f, 0x00, 0x0e, 0x00, 
	0xff, 0xff, 0xff, 0xff, 0xff, 0x7f, 0x38, 0x00, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x71, 0x01, 
	0xff, 0xff, 0xff, 0xdf, 0xff, 0xff, 0xcf, 0x01, 0xff, 0xff, 0xff, 0x1f, 0xfc, 0xff, 0x1f, 0x00, 
	0xff, 0xff, 0xff, 0x1f, 0xf8, 0xf7, 0x1f, 0x00, 0xff, 0xff, 0xff, 0x1f, 0xf8, 0xc3, 0x07, 0x00, 
	0xff, 0xff, 0xff, 0x1f, 0xf8, 0x03, 0x00, 0x00, 0xff, 0xff, 0xff, 0x1f, 0xf8, 0x01, 0x00, 0x00, 
	0xff, 0xff, 0xff, 0x1f, 0xf8, 0x00, 0x00, 0x00, 0xff, 0xff, 0xff, 0x1f, 0xf8, 0x00, 0x00, 0x00, 
	0xff, 0xff, 0xff, 0x1f, 0x78, 0x00, 0x00, 0x00, 0xff, 0xff, 0xff, 0x1f, 0x38, 0x00, 0x00, 0x00, 
	0xff, 0xff, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0xff, 0xff, 0x7f, 0x00, 0x10, 0x00, 0x00, 0x00, 
	0xff, 0xff, 0x7f, 0x00, 0x98, 0x00, 0x00, 0x00, 0xff, 0xff, 0x3f, 0x00, 0x80, 0x01, 0x00, 0x20, 
	0xff, 0xff, 0x2f, 0x00, 0x80, 0x03, 0x00, 0x20, 0xff, 0xff, 0x0f, 0x00, 0x80, 0x03, 0x00, 0x70, 
	0xff, 0xff, 0x1f, 0x00, 0x80, 0x07, 0x00, 0x60, 0xff, 0xff, 0x3f, 0x00, 0x80, 0x0f, 0x00, 0xe0, 
	0xff, 0xff, 0x3f, 0x00, 0xf0, 0x3f, 0x00, 0xe0, 0xff, 0xff, 0x1f, 0x00, 0xfc, 0xff, 0x00, 0xc0, 
	0xff, 0xff, 0x0f, 0x00, 0xfe, 0xff, 0x03, 0xc0, 0xff, 0xff, 0x0f, 0x00, 0xff, 0xff, 0x07, 0x80, 
	0xff, 0x7f, 0x0e, 0x80, 0xff, 0xff, 0x0f, 0x80, 0xff, 0x1f, 0x30, 0xc0, 0xff, 0xff, 0x0f, 0x00, 
	0xff, 0x0f, 0x18, 0xc0, 0xfb, 0xff, 0x1f, 0x00, 0xff, 0x07, 0x0c, 0xe0, 0xe1, 0xff, 0x3f, 0x00, 
	0xff, 0x03, 0x06, 0xf0, 0x80, 0xff, 0x3f, 0x00, 0xff, 0x01, 0x03, 0x70, 0x00, 0xff, 0xff, 0x00, 
	0xff, 0x01, 0x01, 0x78, 0x00, 0xfe, 0xff, 0x00, 0xff, 0x01, 0x00, 0x38, 0x00, 0xfc, 0xff, 0x00, 
	0xff, 0xc1, 0x00, 0x3c, 0x00, 0xfc, 0xff, 0x00, 0xff, 0xe3, 0x00, 0x1c, 0x00, 0xf8, 0xff, 0x01, 
	0xff, 0x63, 0x00, 0x1c, 0x00, 0xf8, 0xff, 0x01, 0xff, 0x61, 0x00, 0x1e, 0x00, 0xf0, 0xff, 0x01, 
	0xff, 0x71, 0x00, 0x0e, 0x00, 0xf0, 0xff, 0x01, 0xff, 0xf9, 0x38, 0x0f, 0x00, 0xe0, 0xff, 0x03, 
	0xff, 0xf8, 0xff, 0x0f, 0x80, 0xe0, 0xff, 0x03, 0xff, 0xfc, 0xff, 0x0f, 0x00, 0xe0, 0xff, 0x01, 
	0xff, 0xfc, 0xff, 0x0f, 0x00, 0xe1, 0xff, 0x03, 0xff, 0xfc, 0xff, 0x0f, 0x80, 0xc5, 0xff, 0x03, 
	0xff, 0xfc, 0xff, 0x07, 0xe0, 0xe3, 0xff, 0x03, 0xff, 0xfe, 0xff, 0x07, 0xe0, 0x89, 0xff, 0x03, 
	0xff, 0xfe, 0xff, 0x07, 0xf0, 0x09, 0xff, 0x03, 0xff, 0xfe, 0xff, 0x07, 0xf0, 0x8e, 0xfc, 0x03, 
	0xff, 0xfc, 0xff, 0x0f, 0x78, 0xb6, 0xff, 0x03, 0xff, 0xfc, 0xff, 0x0f, 0x70, 0xc4, 0xcf, 0x03, 
	0xff, 0xfd, 0xff, 0x0f, 0xe0, 0x10, 0xcf, 0x01, 0xff, 0xfd, 0xff, 0x0f, 0x08, 0x80, 0x8f, 0x03, 
	0xff, 0xf9, 0xff, 0x0f, 0x04, 0x80, 0x07, 0x0a, 0xff, 0xfb, 0xff, 0x0f, 0x0c, 0x00, 0xe3, 0x04, 
	0xff, 0xff, 0xff, 0x1f, 0xf8, 0x00, 0xe0, 0x03, 0xff, 0xff, 0xff, 0x1f, 0xc0, 0x41, 0xe0, 0x0f, 
	0xff, 0xff, 0xff, 0x1f, 0xac, 0xe3, 0xe0, 0x0f, 0xff, 0xff, 0xff, 0x3f, 0xb8, 0xea, 0xf3, 0x0f, 
	0xff, 0xff, 0xff, 0x3f, 0x70, 0xe9, 0xf3, 0x0f, 0xff, 0xff, 0xff, 0x7f, 0xc0, 0x21, 0xf1, 0x0f, 
	0xff, 0xff, 0xff, 0x7f, 0x00, 0x87, 0xf1, 0x0f, 0xff, 0xff, 0xbf, 0xff, 0x00, 0xac, 0xe1, 0x0f, 
	0xff, 0xff, 0xff, 0xff, 0x01, 0xb0, 0x81, 0x07, 0xff, 0xff, 0xff, 0xff, 0x03, 0xc0, 0x03, 0x06, 
	0xff, 0xff, 0xff, 0xff, 0x07, 0x80, 0x07, 0x00, 0xff, 0xff, 0xff, 0xff, 0x1f, 0x07, 0x0e, 0x00, 
	0xff, 0xff, 0xff, 0xff, 0xff, 0x7f, 0xb8, 0x00, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x71, 0x01, 
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xcf, 0x01, 0xff, 0xff, 0xff, 0x1f, 0xfc, 0xff, 0x1f, 0x01, 
	0xff, 0xff, 0xff, 0x1f, 0xf8, 0xff, 0x1f, 0x00, 0xff, 0xff, 0xff, 0x1f, 0xf8, 0xc3, 0x0f, 0x00, 
	0xff, 0xff, 0xff, 0x1f, 0xf8, 0x03, 0x00, 0x00, 0xff, 0xff, 0xff, 0x1f, 0xf8, 0x01, 0x00, 0x00, 
	0xff, 0xff, 0xff, 0x1f, 0xf8, 0x01, 0x00, 0x00, 0xff, 0xff, 0xff, 0x1f, 0xf8, 0x00, 0x00, 0x00, 
	0xff, 0xff, 0xff, 0x1f, 0x78, 0x00, 0x00, 0x00, 0xff, 0xff, 0xff, 0x1f, 0x38, 0x00, 0x00, 0x00
};