#include "CondCore/DBCommon/interface/BlobStreamerPluginFactory.h"
#include "CondCore/DBCommon/interface/Exception.h"
#include "FWCore/MessageLogger/interface/MessageLogger.h"

#include "CondCore/ORA/interface/IBlobStreamingService.h"

#include "TBufferBlobStreamingService.h"


//
#include <cstddef>
//
#include "CoralBase/Blob.h"
//
#include <algorithm>
#include <typeinfo>
#include <string>
#include <cstring>
#include <zlib.h>

namespace cond {

  class BlobStreamingService : virtual public ora::IBlobStreamingService {
    public:

    typedef std::pair<unsigned long long, unsigned long long> uuid;
    
    static const size_t m_idsize=sizeof(uuid);
    static const size_t nVariants=3;

    enum Variant { OLD, COMPRESSED_TBUFFER, COMPRESSED_CHARS }; 
    static uuid const variantIds[nVariants];


    BlobStreamingService();
    
    virtual ~BlobStreamingService();

    boost::shared_ptr<coral::Blob> write( const void* addressOfInputData,  Reflex::Type const & classDictionary );

    void read( const coral::Blob& blobData, void* addressOfContainer,  Reflex::Type const & classDictionary );


  private:

    static Variant findVariant(const void* address);

    static boost::shared_ptr<coral::Blob>  BlobStreamingService::compress(const void* addr, size_t isize);
    static boost::shared_ptr<coral::Blob> BlobStreamingService::expand(const coral::Blob& blobIn);


    boost::shared_ptr<ora::IBlobStreamingService> rootService; 

  };
  
 
  BlobStreamingService::BlobStreamingService() : rootService(new cond::TBufferBlobStreamingService());
  
  ~BlobStreamingService::BlobStreamingService(){}
  
  boost::shared_ptr<coral::Blob> BlobStreamingService::write( const void* addressOfInputData,  Reflex::Type const & classDictionary ) {

    // at the moment we write TBuffer compressed, than we see....
    // we may wish to avoid one buffer copy...
    boost::shared_ptr<coral::Blob> buffer = rootService->write(addressOfInputData, classDictionary);
    boost::shared_ptr<coral::Blob> blobOut = compress(buffer->startingAddress(),buffer->size());
    *reinterpret_cast<uuid*>(blobOut->startingAddress()) = variantIds[COMPRESSED_TBUFFER];
  }

  void BlobStreamingService::read( const coral::Blob& blobData, void* addressOfContainer,  Reflex::Type const & classDictionary ) {
    Variant v = findVariant(blobData->startingAddress());
    switch (v) {
    case OLD :
      rootService->read( blobData, addressOfContainer, classDictionary)
      break;
    case COMPRESSED_TBUFFER :
      boost::shared_ptr<coral::Blob> blobIn = expand(blobData);
      rootService->read( blobIn, addressOfContainer, classDictionary)
	break;
    case COMPRESSED_CHARS :
      break;
    }


  }




 const BlobStreamingService::uuid BlobStreamingService::variantIds[nVariants] = {
    BlobStreamingService::uuid(0,0)
    ,BlobStreamingService::uuid(0xf4e92f169c974e8e, 0x97851f372586010d)
    ,BlobStreamingService::uuid(0xc9a95a45e60243cf, 0x8dc549534f9a9274)
  };


  BlobStreamingService::Variant BlobStreamingService::findVariant(const void* address) {
    uuid const & id = *reinterpret_cast<uuid const*>(address);
    uuid const *  v = std::find(variantIds,variantIds+nvariants,id);
    return v==variantIds+nvariants ? OLD : (Variant)(v-variantIds);
  }


   boost::shared_ptr<coral::Blob> BlobStreamingService::compress(const void* addr, size_t isize) {
     size_t usize += m_idsize + sizeof(unsigned long long);
     boost::shared_ptr<coral::Blob> theBlob( new coral::Blob(usize));
     uLongf destLen = compressBound(isize);
     void *startingAddress = theBlob->startingAddress()+ m_idsize + sizeof(unsigned long long);
     int zerr =  compress2(startingAddress, &destLen,
			   addr, isize,
			   9);
     if (zerr!=0) edm::LogError("BlobStreamingService")<< "Compression error " << zerr;
     destLen+= m_idsize + sizeof(unsigned long long);
     theBlob->resize(destLen);
     void *startingAddress = theBlob->startingAddress()+ m_idsize;
     // write expanded size;
     *reinterpret_cast<unsigned long long*>(startingAddress)=isize; 
     return theBlob;
  }

  boost::shared_ptr<coral::Blob> BlobStreamingService::expand(const coral::Blob& blobIn) {
    if (blobIn.size()< m_idsize + sizeof(unsigned long long)) return;
    long long csize =  blobIn.size() - (m_idsize + sizeof(unsigned long long));
    void const * startingAddress = blobIn->startingAddress()+ m_idsize;
    unsigned long long usize = *reinterpret_cast<unsigned long long const*>(startingAddress);
    startingAddress += sizeof(unsigned long long);
    boost::shared_ptr<coral::Blob> theBlob( new coral::Blob(usize));
    uLongf destLen = usize;
    int zerr =  uncompress(theBlob->startingAddress(),  &destLen,
			   startingAddress, usize);
    if (zerr!=0 || usize!=destLen) 
      edm::LogError("BlobStreamingService")<< "uncompressing error " << zerr
			       << " original size was " << usize
			       << " new size is " << destLen;
    
     return theBlob; 
  }

}

// keep the old good name
DEFINE_EDM_PLUGIN(cond::BlobStreamerPluginFactory,cond::BlobStreamerService,"COND/Services/TBufferBlobStreamingService");
