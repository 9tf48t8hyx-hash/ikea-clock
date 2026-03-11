#include "screen.h"
#include "constants.h"
#include <SPI.h>
#include <algorithm>

#define TIMER_INTERVAL_US 200
#define GRAY_LEVELS 64

using namespace std;

uint8_t Screen_::getCurrentBrightness() const { return brightness_; }

void Screen_::setBrightness(uint8_t brightness, bool shouldStore)
{
  brightness_ = brightness;
  pinMode(PIN_ENABLE, OUTPUT);
  digitalWrite(PIN_ENABLE, LOW);
}

void Screen_::setRenderBuffer(const uint8_t *renderBuffer, bool grays)
{
  if (grays)
    memcpy(renderBuffer_, renderBuffer, ROWS * COLS);
  else
    for (int i = 0; i < ROWS * COLS; i++)
      renderBuffer_[i] = renderBuffer[i] * MAX_BRIGHTNESS;
}

uint8_t *Screen_::getRenderBuffer() { return renderBuffer_; }
uint8_t  Screen_::getBufferIndex(int index) { return renderBuffer_[index]; }

void Screen_::clear() { memset(renderBuffer_, 0, ROWS * COLS); }

void Screen_::clearRect(int x, int y, int width, int height)
{
  if (x < 0) { width += x; x = 0; }
  if (y < 0) { height += y; y = 0; }
  if (x >= COLS || y >= ROWS || width <= 0 || height <= 0) return;
  width = std::min(width, COLS - x);
  for (int row = y; row < y + height; row++)
    memset(renderBuffer_ + (row * COLS + x), 0, width);
}

void Screen_::setup()
{
  Screen.setCurrentRotation(0);
  pinMode(PIN_LATCH, OUTPUT);
  pinMode(PIN_ENABLE, OUTPUT);
  digitalWrite(PIN_LATCH, LOW);
  digitalWrite(PIN_ENABLE, LOW);
  SPI.begin(PIN_CLOCK, -1, PIN_DATA, -1);
  SPI.beginTransaction(SPISettings(10000000, MSBFIRST, SPI_MODE0));
  hw_timer_t *t = timerBegin(1000000);
  timerAttachInterrupt(t, &onScreenTimer);
  timerAlarm(t, TIMER_INTERVAL_US, true, 0);
}

void Screen_::setPixelAtIndex(uint8_t index, uint8_t value, uint8_t brightness)
{
  if (index >= COLS * ROWS) return;
  renderBuffer_[index] = value <= 0 || brightness <= 0 ? 0
                       : (brightness > MAX_BRIGHTNESS ? MAX_BRIGHTNESS : brightness);
}

void Screen_::setPixel(uint8_t x, uint8_t y, uint8_t value, uint8_t brightness)
{
  if (x >= COLS || y >= ROWS) return;
  renderBuffer_[y * COLS + x] = value <= 0 || brightness <= 0 ? 0
                              : (brightness > MAX_BRIGHTNESS ? MAX_BRIGHTNESS : brightness);
}

void Screen_::setCurrentRotation(int rotation, bool shouldPersist)
{
  currentRotation = rotation & 0x3;
}

IRAM_ATTR uint8_t *Screen_::getRotatedRenderBuffer()
{
  if (currentRotation == 0) return renderBuffer_;
  for (int i = 0; i < ROWS * COLS; i++) rotatedRenderBuffer_[i] = renderBuffer_[i];
  rotate();
  return rotatedRenderBuffer_;
}

IRAM_ATTR void Screen_::rotate()
{
  uint8_t temp[TOTAL_PIXELS];
  if (currentRotation == 1) {
    for (int r = 0; r < ROWS; r++)
      for (int c = 0; c < COLS; c++)
        temp[c * COLS + (ROWS - 1 - r)] = rotatedRenderBuffer_[r * COLS + c];
    memcpy(rotatedRenderBuffer_, temp, TOTAL_PIXELS);
  } else if (currentRotation == 2) {
    for (int i = 0; i < TOTAL_PIXELS / 2; i++)
      swap(rotatedRenderBuffer_[i], rotatedRenderBuffer_[TOTAL_PIXELS - 1 - i]);
  } else if (currentRotation == 3) {
    for (int r = 0; r < ROWS; r++)
      for (int c = 0; c < COLS; c++)
        temp[(COLS - 1 - c) * COLS + r] = rotatedRenderBuffer_[r * COLS + c];
    memcpy(rotatedRenderBuffer_, temp, TOTAL_PIXELS);
  }
}

IRAM_ATTR void Screen_::onScreenTimer() { Screen._render(); }

IRAM_ATTR void Screen_::_render()
{
  const auto buf = (currentStatus == UPDATE) ? renderBuffer_ : getRotatedRenderBuffer();
  static unsigned long spi_bits[(ROWS * COLS + 8 * sizeof(unsigned long) - 1) / 8 / sizeof(unsigned long)] = {0};
  unsigned char *bits = (unsigned char *)spi_bits;
  memset(bits, 0, ROWS * COLS / 8);
  static unsigned char counter = 0;
  if (currentStatus == UPDATE) {
    for (int idx = 0; idx < ROWS * COLS; idx++)
      if (buf[positions[idx]] > 0) bits[idx >> 3] |= (0x80 >> (idx & 7));
  } else {
    for (int idx = 0; idx < ROWS * COLS; idx++) {
      uint16_t sv = ((uint16_t)buf[positions[idx]] * brightness_) / MAX_BRIGHTNESS;
      bits[idx >> 3] |= (sv > counter ? 0x80 : 0) >> (idx & 7);
    }
    counter += ((MAX_BRIGHTNESS + 1) / GRAY_LEVELS);
  }
  digitalWrite(PIN_LATCH, LOW);
  SPI.writeBytes(bits, sizeof(spi_bits));
  digitalWrite(PIN_LATCH, HIGH);
}

void Screen_::drawLine(int x1, int y1, int x2, int y2, int ledStatus, uint8_t brightness)
{
  int dx = abs(x2-x1), sx = x1<x2 ? 1:-1;
  int dy = abs(y2-y1), sy = y1<y2 ? 1:-1;
  int error = (dx>dy ? dx:-dy)/2;
  while (x1!=x2 || y1!=y2) {
    setPixel(x1,y1,ledStatus,brightness);
    int e2=error;
    if (e2>-dx){error-=dy; x1+=sx; setPixel(x1,y1,ledStatus,brightness);}
    else if(e2<dy){error+=dx; y1+=sy; setPixel(x1,y1,ledStatus,brightness);}
  }
}

void Screen_::drawRectangle(int x,int y,int w,int h,bool fill,int st,uint8_t br)
{
  if (!fill) {
    drawLine(x,y,x+w,y,st,br); drawLine(x,y+1,x,y+h-1,st,br);
    drawLine(x+w,y+1,x+w,y+h-1,st,br); drawLine(x,y+h-1,x+w,y+h-1,st,br);
  } else {
    for (int i=x; i<x+w; i++) drawLine(i,y,i,y+h-1,st,br);
  }
}

void Screen_::drawCharacter(int x,int y,const std::vector<int>&bits,int bitCount,uint8_t br)
{
  for (int i=0; i<(int)bits.size(); i+=bitCount)
    for (int j=0; j<bitCount; j++)
      setPixel(x+j, y+(i/bitCount), bits[i+j], br);
}

std::vector<int> Screen_::readBytes(const std::vector<int>&bytes)
{
  vector<int> bits;
  for (int b : bytes)
    for (int j=7; j>=0; j--) bits.push_back((b>>j)&1);
  return bits;
}

void Screen_::drawNumbers(int x,int y,const std::vector<int>&numbers,uint8_t br)
{
  for (int i=0; i<(int)numbers.size(); i++)
    drawCharacter(x+(i*5), y, readBytes(smallNumbers[numbers[i]]), 4, br);
}

void Screen_::drawBigNumbers(int x,int y,const std::vector<int>&numbers,uint8_t br)
{
  for (int i=0; i<(int)numbers.size(); i++)
    drawCharacter(x+(i*8), y, readBytes(bigNumbers[numbers[i]]), 8, br);
}

void Screen_::drawWeather(int x,int y,int weather,uint8_t br)
{
  drawCharacter(x, y, readBytes(weatherIcons[weather]), 16, br);
}

void Screen_::scrollText(const std::string&text,int delayTime,uint8_t br,uint8_t fontid)
{
  font cf = (fontid<fonts.size()) ? fonts[fontid] : fonts[0];
  int tw = text.length()*(cf.sizeX+1);
  for (int i=-ROWS; i<tw; i++) {
    int skip=0; clear();
    for (size_t s=0; s<text.length(); s++) {
      if (text[s]==195){skip++;continue;}
      int xp=(s-skip)*(cf.sizeX+1)-i;
      if (xp>-6 && xp<ROWS) {
        uint8_t c=(((text[s]-cf.offset)<(int)cf.data.size())&&(text[s]>=cf.offset))?text[s]:cf.offset;
        Screen.drawCharacter(xp,4,Screen.readBytes(cf.data[c-cf.offset]),8);
      }
    }
    vTaskDelay(pdMS_TO_TICKS(delayTime));
  }
}

void Screen_::scrollGraph(const std::vector<int>&graph,int miny,int maxy,int delay_,uint8_t br)
{
  if (graph.empty()) return;
  for (int i=-ROWS; i<(int)graph.size(); i++) {
    clear(); int y1=-999;
    for (int x=0; x<ROWS; x++) {
      int idx=i+x;
      if (idx>=0&&idx<(int)graph.size()) {
        int y2=ROWS-((graph[idx]-miny+1)*ROWS)/(maxy-miny+1);
        if (x>0&&idx>0&&abs(y2-y1)<6) drawLine(x-1,y1,x,y2,1,br);
        else setPixel(x,y2,1,br);
        y1=y2;
      }
    }
    vTaskDelay(pdMS_TO_TICKS(delay_));
  }
}

Screen_ &Screen_::getInstance() { static Screen_ i; return i; }
Screen_ &Screen = Screen.getInstance();
